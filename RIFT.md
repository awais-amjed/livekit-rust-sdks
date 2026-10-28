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
