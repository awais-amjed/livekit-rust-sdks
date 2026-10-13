# The `rift-crates` branch

LiveKit's Rust SDKs at the commit that released `livekit` 0.10.1, `libwebrtc`
0.4.0 and `webrtc-sys` 0.3.53 (`88705b15`), with the one change the
[Rift](https://joinrift.app) bot SDK needs, packaged for crates.io. Everything
else is upstream, unchanged.

1. **`livekit`: a two-channel audio track is published in stereo.** The server
   writes `stereo=1` into its answer only when `AddTrackRequest` lists
   `TF_STEREO`, and this SDK never listed it, so every track published from
   Rust was mono on an encrypted connection (issue
   [#1018](https://github.com/livekit/rust-sdks/issues/1018); the same change
   as PR [#1023](https://github.com/livekit/rust-sdks/pull/1023)). Measured
   Oct 12 2026: a tone in one ear came out in both before, in one after.

The `livekit` package is renamed `joinrift-livekit` (the library is still
`livekit`) and has its own README, so it can be published without passing for
LiveKit's release. Its other LiveKit dependencies come from crates.io as LiveKit
released them.

## Why not all of `rift`

The `rift` branch carries this change too, and others the Rift app needs for
screen sharing. Since livekit 0.10.1 some of those change `libwebrtc` and
`webrtc-sys` as well as `livekit`, so a `livekit` built from `rift` no longer
compiles against LiveKit's released `libwebrtc`. A bot only publishes and hears
audio, so this branch is the release commit plus the stereo change alone.

## Moving to a newer release

Start a branch at LiveKit's release commit, cherry-pick the stereo change and
the commit that renames the package, set the version to the release's, and
publish `joinrift-livekit` from this branch.

The SDKs are LiveKit's, under the Apache-2.0 licence in `LICENSE`. This is
Rift's fork, not an official LiveKit release, and LiveKit has not reviewed or
endorsed it. Each changed file marks what changed with `RIFT PATCH`.
