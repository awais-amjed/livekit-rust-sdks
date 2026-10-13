# The `rift` branch

LiveKit's Rust SDKs at the commit that released `livekit` 0.9.1 and
`webrtc-sys` 0.3.45 (`2ff180e4`), with two changes [Rift](https://joinrift.app)
needs. Everything else is upstream, unchanged.

1. **`livekit`: a two-channel audio track is published in stereo.** The server
   writes `stereo=1` into its answer only when `AddTrackRequest` lists
   `TF_STEREO`, and this SDK never listed it, so every track published from
   Rust was mono on an encrypted connection (issue
   [#1018](https://github.com/livekit/rust-sdks/issues/1018); the same change
   as PR [#1023](https://github.com/livekit/rust-sdks/pull/1023)). Measured
   Oct 12 2026: a tone in one ear came out in both before, in one after.
2. **`webrtc-sys`: WebRTC trusts the pass-through encoder's rate control.**
   Frames handed over already encoded were still subject to WebRTC's frame
   dropper, and one dropped H264 delta frame froze the picture until the next
   keyframe. Measured Oct 5 2026 on Windows: 4 to 7 frames dropped a run
   before, none after.

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

The SDKs are LiveKit's, under the Apache-2.0 licence in `LICENSE`.
