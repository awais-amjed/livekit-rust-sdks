/*
 * Copyright 2025 LiveKit, Inc.
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

#include "livekit/desktop_capturer.h"

#include "modules/desktop_capture/desktop_capture_options.h"
#ifdef _WIN64
// RIFT PATCH: for a screen captured without Windows Graphics Capture.
#include "modules/desktop_capture/desktop_and_cursor_composer.h"
#endif /* _WIN64 */
#ifdef WEBRTC_USE_PIPEWIRE
// RIFT PATCH: for select_restore_token.
#include "modules/desktop_capture/linux/wayland/restore_token_manager.h"
#endif /* WEBRTC_USE_PIPEWIRE */

using SourceList = webrtc::DesktopCapturer::SourceList;

namespace livekit_ffi {

std::unique_ptr<DesktopCapturer> new_desktop_capturer(
    DesktopCapturerOptions options) {
  webrtc::DesktopCaptureOptions webrtc_options =
      webrtc::DesktopCaptureOptions::CreateDefault();
#if defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)
  webrtc_options.set_allow_sck_capturer(true);
  webrtc_options.set_allow_sck_system_picker(options.allow_sck_system_picker);
#endif /* defined(WEBRTC_MAC) && !defined(WEBRTC_IOS) */
#ifdef _WIN64
  switch (options.source_type) {
    case SourceType::Screen:
      // RIFT PATCH: Windows Graphics Capture draws a yellow border round the
      // screen, which only Windows 11 lets an app turn off. Without it a
      // screen is captured with DXGI desktop duplication, falling back to GDI.
      webrtc_options.set_allow_wgc_screen_capturer(
          options.allow_wgc_screen_capturer);
      break;
    case SourceType::Window:
      webrtc_options.set_allow_wgc_window_capturer(true);
      // https://github.com/webrtc-sdk/webrtc/blob/m137_release/modules/desktop_capture/desktop_capture_options.h#L133-L142
      webrtc_options.set_enumerate_current_process_windows(false);
      break;
    default:
      break;
  }
  webrtc_options.set_allow_directx_capturer(true);
#endif /* _WIN64 */
#ifdef WEBRTC_USE_PIPEWIRE
  webrtc_options.set_allow_pipewire(true);
#endif /* WEBRTC_USE_PIPEWIRE */

  // prefer_cursor_embedded indicate that the capturer should try to include the
  // cursor in the frame
  webrtc_options.set_prefer_cursor_embedded(options.include_cursor);

  std::unique_ptr<webrtc::DesktopCapturer> capturer = nullptr;
  switch (options.source_type) {
    case SourceType::Window:
      capturer = webrtc::DesktopCapturer::CreateWindowCapturer(webrtc_options);
      break;
    case SourceType::Screen:
      capturer = webrtc::DesktopCapturer::CreateScreenCapturer(webrtc_options);
#ifdef _WIN64
      // RIFT PATCH: only Windows Graphics Capture puts the cursor in its
      // frames (prefer_cursor_embedded); DXGI and GDI leave it out, so it
      // is drawn in.
      if (capturer && options.include_cursor &&
          !options.allow_wgc_screen_capturer) {
        capturer = std::make_unique<webrtc::DesktopAndCursorComposer>(
            std::move(capturer), webrtc_options);
      }
#endif /* _WIN64 */
      break;
    case SourceType::Generic:
      capturer = webrtc::DesktopCapturer::CreateGenericCapturer(webrtc_options);
      break;
    default:
      return nullptr;
  }

  if (!capturer) {
    return nullptr;
  }
  return std::make_unique<DesktopCapturer>(std::move(capturer));
}

void DesktopCapturer::start(
    rust::Box<DesktopCapturerCallbackWrapper> callback) {
  this->callback = std::move(callback);
  capturer->Start(this);
}

void DesktopCapturer::OnCaptureResult(
    webrtc::DesktopCapturer::Result result,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  CaptureResult ret_result = CaptureResult::ErrorPermanent;
  switch (result) {
    case webrtc::DesktopCapturer::Result::SUCCESS:
      ret_result = CaptureResult::Success;
      break;
    case webrtc::DesktopCapturer::Result::ERROR_PERMANENT:
      ret_result = CaptureResult::ErrorPermanent;
      break;
    case webrtc::DesktopCapturer::Result::ERROR_TEMPORARY:
      ret_result = CaptureResult::ErrorTemporary;
      break;
    default:
      break;
  }
  if (callback) {
    (*callback)->on_capture_result(
        ret_result, std::make_unique<DesktopFrame>(std::move(frame)));
  }
}

// RIFT PATCH: the PipeWire capturer opens the portal with the restore token
// saved under its selected source, so the desktop shows no picker and shares
// what the token names. An app that ran the portal itself (to learn which
// window was picked) hands the choice over this way. A fresh id, since the
// manager never replaces a token it holds. Where the capturer is not the
// portal's (X11, Windows, macOS) there is no token to use.
bool DesktopCapturer::select_restore_token(rust::Str token) const {
#ifdef WEBRTC_USE_PIPEWIRE
  if (!webrtc::DesktopCapturer::IsRunningUnderWayland()) {
    return false;
  }
  auto& tokens = webrtc::RestoreTokenManager::GetInstance();
  const webrtc::DesktopCapturer::SourceId id = tokens.GetUnusedId();
  tokens.AddToken(id, std::string(token));
  return capturer->SelectSource(id);
#else
  return false;
#endif /* WEBRTC_USE_PIPEWIRE */
}

rust::Vec<Source> DesktopCapturer::get_source_list() const {
  SourceList list{};
  bool res = capturer->GetSourceList(&list);
  rust::Vec<Source> source_list{};
  if (res) {
    for (auto& source : list) {
      source_list.push_back(Source{static_cast<uint64_t>(source.id),
                                   source.title, source.display_id});
    }
  }
  return source_list;
}
}  // namespace livekit_ffi