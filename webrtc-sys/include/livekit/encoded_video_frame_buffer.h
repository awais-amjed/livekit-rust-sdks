/*
 * Copyright 2026 LiveKit, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>

#include "api/video/encoded_image.h"
#include "api/video/video_frame_buffer.h"

namespace livekit {

enum class EncodedVideoCodec {
  kH264,
  kH265,
  kVP8,
  kVP9,
  kAV1,
};

enum class EncodedFrameType {
  kKey,
  kDelta,
};

struct EncodedRateControlRequest {
  bool has_request = false;
  uint64_t target_bitrate_bps = 0;
  double framerate_fps = 0.0;
};

// Latest-wins rate-control mailbox shared between the pass-through encoder and
// the Rust capture side.
class EncodedRateControlState {
 public:
  void Store(uint64_t target_bitrate_bps, double framerate_fps);
  EncodedRateControlRequest Take();

 private:
  std::mutex mutex_;
  EncodedRateControlRequest request_;
};

// RIFT PATCH: hands each pre-encoded frame from a source to the pass-through
// encoder.
//
// libwebrtc's encoder thread drops the older of two frames queued for it
// ("encoder blocked"), which costs raw frames nothing. A pre-encoded delta
// frame needs every frame before it, so each drop breaks the chain until a
// keyframe. A source therefore waits, briefly, for the encoder to take a
// frame before handing over the next. Also counts the chain breaks, for the
// capture side to log, and says when a keyframe is wanted to mend one, which
// must go at once, apart from the keyframes viewers ask for, which the
// capture side may space out.
class EncodedHandover {
 public:
  // The pass-through encoder took frame `sequence_number`.
  void Taken(uint64_t sequence_number);

  // Called before handing over `sequence_number`: waits up to `most` for the
  // frame before it to be taken. Only while the encoder is keeping up, having
  // taken the frame before that, so a paused encoder costs one wait. A wait
  // that runs out rests the waiting for `rest`: the congestion window drops
  // a frame in every few, each of which would cost a whole wait.
  void WaitForPrevious(uint64_t sequence_number,
                       std::chrono::microseconds most,
                       std::chrono::microseconds rest);

  // The chain broke before the pass-through encoder: a frame, or several in a
  // row, never reached it, and the deltas after were held until a keyframe.
  void ChainBroken();
  // Returns and clears the chain breaks counted since the last call.
  uint32_t TakeChainBreaks();

  // The pass-through encoder is holding deltas until a keyframe.
  void RepairWanted() { repair_wanted_.store(true, std::memory_order_relaxed); }
  // Returns and clears whether a keyframe is wanted to mend the chain.
  bool TakeRepair() {
    return repair_wanted_.exchange(false, std::memory_order_relaxed);
  }

 private:
  std::mutex mutex_;
  std::condition_variable taken_;
  bool any_taken_ = false;
  uint64_t last_taken_ = 0;
  std::chrono::steady_clock::time_point rest_until_;
  uint32_t chain_breaks_ = 0;
  std::atomic<bool> repair_wanted_{false};
};

// A native WebRTC frame buffer carrying one encoded video access unit.
class EncodedVideoFrameBuffer : public webrtc::VideoFrameBuffer {
 public:
  // `keyframe_request_flag` is shared with the owning video source: the
  // pass-through encoder sets it when the RTP layer asks for a keyframe the
  // pending frame cannot satisfy, and the capture side polls it to forward
  // the request upstream.
  EncodedVideoFrameBuffer(
      int width,
      int height,
      EncodedVideoCodec codec,
      EncodedFrameType frame_type,
      webrtc::scoped_refptr<webrtc::EncodedImageBufferInterface> payload,
      std::shared_ptr<std::atomic<bool>> keyframe_request_flag = nullptr,
      std::shared_ptr<EncodedRateControlState> rate_control_state = nullptr,
      std::shared_ptr<EncodedHandover> handover = nullptr);
  ~EncodedVideoFrameBuffer() override = default;

  Type type() const override;
  int width() const override;
  int height() const override;
  webrtc::scoped_refptr<webrtc::I420BufferInterface> ToI420() override;
  webrtc::scoped_refptr<webrtc::VideoFrameBuffer> CropAndScale(
      int offset_x,
      int offset_y,
      int crop_width,
      int crop_height,
      int scaled_width,
      int scaled_height) override;

  EncodedVideoCodec codec() const { return codec_; }
  EncodedFrameType frame_type() const { return frame_type_; }

  // The encoded access unit. Shared with the pass-through encoder so the
  // payload is not copied again on the send path.
  webrtc::scoped_refptr<webrtc::EncodedImageBufferInterface> encoded_data() const {
    return payload_;
  }
  const uint8_t* payload_data() const { return payload_->data(); }
  size_t payload_size() const { return payload_->size(); }

  // Identifies the source that delivered this frame.
  const void* stream_id() const { return stream_id_; }
  // Delivery order within `stream_id()`; the pass-through encoder uses it to
  // detect frames WebRTC dropped before they reached it.
  uint64_t sequence_number() const { return sequence_number_; }
  // Set by the source as it delivers the frame, before anything else sees it.
  void set_stream_position(const void* stream_id, uint64_t sequence_number) {
    stream_id_ = stream_id;
    sequence_number_ = sequence_number;
  }

  // Asks the capture side to produce a keyframe (e.g. on PLI/FIR).
  void request_keyframe() const;

  // Updates the capture side with the latest encoder rate-control target.
  void set_rate_control_request(uint64_t target_bitrate_bps,
                                double framerate_fps) const;
  // RIFT PATCH: the source's rate mailbox, for the pass-through encoder to
  // keep, so a rate set between frames reaches the source at once.
  std::shared_ptr<EncodedRateControlState> rate_control_state() const {
    return rate_control_state_;
  }

  // RIFT PATCH: tells the source the pass-through encoder took this frame.
  void taken() const;
  // Tells the source this frame follows one that never reached the encoder.
  void chain_broken() const;
  // Asks the capture side for a keyframe to mend the chain: this frame is
  // held, as is every delta until one comes. Sets the keyframe request too.
  void repair_chain() const;

  static EncodedVideoFrameBuffer* FromNative(webrtc::VideoFrameBuffer* buffer);

 private:
  int width_;
  int height_;
  EncodedVideoCodec codec_;
  EncodedFrameType frame_type_;
  webrtc::scoped_refptr<webrtc::EncodedImageBufferInterface> payload_;
  std::shared_ptr<std::atomic<bool>> keyframe_request_flag_;
  std::shared_ptr<EncodedRateControlState> rate_control_state_;
  std::shared_ptr<EncodedHandover> handover_;
  const void* stream_id_ = nullptr;
  uint64_t sequence_number_ = 0;
};

}  // namespace livekit
