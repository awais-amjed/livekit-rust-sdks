# The `rift` branch

LiveKit's Rust SDKs at the commit that released `livekit` 0.9.5 and
`webrtc-sys` 0.3.49 (`49b3ebe9`), with the changes [Rift](https://joinrift.app)
needs. Everything else is upstream, unchanged.

1. **`livekit`: a two-channel audio track is published in stereo.** The server
   writes `stereo=1` into its answer only when `AddTrackRequest` lists
   `TF_STEREO`, and this SDK never listed it, so every track published from
   Rust was mono on an encrypted connection (issue
   [#1018](https://github.com/livekit/rust-sdks/issues/1018); the same change
   as PR [#1023](https://github.com/livekit/rust-sdks/pull/1023)). Measured
   Oct 12 2026: a tone in one ear came out in both before, in one after.

2. **`libwebrtc`: a desktop capture can start from a choice the ScreenCast
   portal saved** (`DesktopCapturer::select_restore_token`). Under Wayland
   libwebrtc runs the portal's picker itself, and the portal tells an app
   nothing about the window picked. Rift runs the picker first, keeping the
   pick, to read which app's window it was (GNOME saves the app id and title
   with it), then hands libwebrtc the pick's restore token, which it already
   knows how to use: `RestoreTokenManager`, the prebuilt library's own. No
   second picker. Verified Oct 14 2026 on GNOME 50: the window shared 0.07 s
   after the one pick.

3. **`libwebrtc`: a Windows screen can be captured without Windows Graphics
   Capture** (`DesktopCapturerOptions::set_wgc_screen_capturer`). Windows
   draws a yellow border round a screen captured that way, and only Windows
   11 lets an app turn it off, which libwebrtc does. Turned off, a screen is
   captured with DXGI desktop duplication (GDI where that fails), which draws
   none, and `webrtc-sys` puts the cursor into its frames with
   `DesktopAndCursorComposer`, since only Windows Graphics Capture draws it
   itself. Rift turns it off where Windows cannot hide the border.

4. **`webrtc-sys` and `livekit`: a pre-encoded H264 track can be sent in
   High profile.** The pass-through encoder offered only constrained baseline
   (`42e01f`). Three changes, each needed (verified Oct 28 2026 against
   livekit-server 1.13.7, NVENC and VAAPI, encrypted and not):
   - The pass-through offers High as `640032`, the server's own; the server
     matches H264 on profile, not level. `TrackPublishOptions::h264_high`
     makes it the only H264 the transceiver offers: the server answers in its
     own order, constrained baseline first, so offered beside it High was
     never chosen. A server without High refuses such a track. Without the
     option nothing changes.
   - The decoder factory lists High too; its FFmpeg decoder plays it. The
     server settles a connection's codecs from its first video section, a
     receiving one the SDK adds up front, so without High there a High track
     published later was answered with constrained baseline's payload type.
   - The encoder factory's implementations list the pass-through's formats.
     WebRTC sends only a negotiated codec it finds there; baseline was there
     through the software encoder and High was not ("No video codecs in
     common", nothing sent).

5. **Pass-through decoding, taken from upstream before it is merged.** PR
   [#1515](https://github.com/livekit/rust-sdks/pull/1515) (open upstream,
   its ten commits as they were on Oct 27 2026, unchanged): a factory made
   with `VideoDecoderBackend::PassThrough` decodes nothing, and a remote video
   track's frames are read still encoded, already decrypted, through
   `NativeEncodedVideoStream`. Rift decodes them itself on the GPU. On top of
   it, one change of our own: the shared runtime every `Room` uses can be made
   that way (`livekit::webrtc::set_video_decoder_backend`, before the first
   room, the way `enable_zero_playout_delay` is chosen), keeping its WARP
   options. A runtime is one factory with its own threads, so a second
   factory for one room would mix objects made on two sets of threads.
   Rift's process holds two kinds of room on it: its share connections,
   which only send, and the one it watches shares on. And one fix to the PR:
   a pass-through factory turns pre-render smoothing off when it makes a
   connection, and libwebrtc refuses any later change to it, so the
   configuration LiveKit's Room sets again on joining was refused
   ("Modifying the configuration in an unsupported way") and no room
   joined. `set_configuration` now keeps the connection's smoothing when the
   configuration leaves it at its default. Verified Nov 2 2026 against
   livekit-server: encrypted VP8, VP9 and H264 shares read still encoded and
   decoded by Rift on an Intel GPU, none lost. And a second fix to the PR,
   for Windows: its test helper `passthrough_decode_for_test` is compiled
   into every build, and returned a `rust::Vec` of a bridge struct whose
   Rust side only a test build has. MSVC's linker wants that side anyway
   (LNK2019, the PR's own Windows CI failure), where Linux's drops the unused
   helper. It now returns eight numbers per decode, which the test reads back
   into the same struct. Verified Nov 4 2026: Rift's library linked for
   Windows (cargo-xwin, lld-link) where it failed before, and the decoder's
   tests pass on Linux.

6. **`webrtc-sys`: a pre-encoded H265 track can be sent where no real encoder
   lists H265.** The encoder factory advertised a pass-through format only for
   a codec some real encoder has, and on Linux and Windows without LiveKit's
   NVENC none has H265. H265 now passes that filter, by its one name WebRTC
   knows (`H265`, not `HEVC`). A track asks for its own codec alone, so only a
   pre-encoded track that asked for H265 negotiates it. The prebuilt libwebrtc
   packetizes it and its frame cryptor handles it. Verified Nov 7 2026 against
   livekit-server 1.13.7: encrypted H265 from FFmpeg's `hevc_vaapi` (Intel) and
   `hevc_nvenc`, read still encoded and decoded by Rift on an Intel GPU, 40
   keyframes in 20 s with every picture drawn.

7. **`livekit`: a local participant can read the codecs the server takes**
   (`LocalParticipant::enabled_publish_codecs`), from the join response, which
   the SDK otherwise drops. A server with H265 off does not refuse a track
   published in H265: `publish_track` succeeds, the server lists the track as
   VP8, and viewers get no picture (seen Nov 7 2026 on livekit-server 1.13 with
   `room.enabled_codecs`). So a publisher has to ask first.

8. **`livekit`: an H265 track gets the start bitrate hint.** libwebrtc reads
   `x-google-start-bitrate` from the answer. The server repeats the offer's
   parameters for H264 but answers H265 with its own, so an H265 track started
   at libwebrtc's 300 kbps (livekit-server 1.13, seen Nov 8 2026). On a still
   screen its estimate then fell to the floor with nothing to climb back to, and
   a picture that moved took most of a minute to reach its rate. The hint now goes
   into the answer's H265 too, capped at 1 Mbps: from 3 Mbps up the first probes
   go out at 5 Mbps, which a slow upload loses. It can go when the server repeats
   H265's parameters as it does H264's.

The branch on 0.9.1 also changed `webrtc-sys` so WebRTC trusts the
pass-through encoder's rate control. 0.3.49 does that itself and more
([#1511](https://github.com/livekit/rust-sdks/pull/1511)), so that change was
dropped. The earlier branch is kept as the tag `rift-livekit-0.9.1`.

## Using it

```toml
[patch.crates-io]
livekit = { git = "https://github.com/awais-amjed/livekit-rust-sdks", rev = "<commit>" }
webrtc-sys = { git = "https://github.com/awais-amjed/livekit-rust-sdks", rev = "<commit>" }
```

Pin a commit of this branch rather than the branch itself. The sibling crates
(`libwebrtc`, `livekit-api`, `livekit-protocol` and the rest) then come from
the same commit, so they always match.

## When it goes

Each change goes when a LiveKit release has it, or an equivalent. Until then,
moving to a newer release means rebasing this branch onto its release commit.

The SDKs are LiveKit's, under the Apache-2.0 licence in `LICENSE`. This is
Rift's fork, not an official LiveKit release, and LiveKit has not reviewed or
endorsed it. Each changed file marks what changed with `RIFT PATCH`.
