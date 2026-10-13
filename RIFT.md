# The `rift` branch

LiveKit's Rust SDKs at the commit that released `livekit` 0.9.5 and
`webrtc-sys` 0.3.49 (`49b3ebe9`), with one change [Rift](https://joinrift.app)
needs. Everything else is upstream, unchanged.

1. **`livekit`: a two-channel audio track is published in stereo.** The server
   writes `stereo=1` into its answer only when `AddTrackRequest` lists
   `TF_STEREO`, and this SDK never listed it, so every track published from
   Rust was mono on an encrypted connection (issue
   [#1018](https://github.com/livekit/rust-sdks/issues/1018); the same change
   as PR [#1023](https://github.com/livekit/rust-sdks/pull/1023)). Measured
   Oct 12 2026: a tone in one ear came out in both before, in one after.

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

## On crates.io

The `rift-crates` branch is `rift` plus one commit that renames the `livekit`
package to `joinrift-livekit` (the library is still `livekit`) and gives it its
own README, so it can be published without passing for LiveKit's release. The
crate's other LiveKit dependencies come from crates.io as LiveKit released them,
since none of them is changed. `rift` itself keeps the name `livekit`, which
`[patch.crates-io]` needs. Publish from `rift-crates`, after rebasing it onto
`rift`.

## When it goes

Each change goes when a LiveKit release has it, or an equivalent. Until then,
moving to a newer release means rebasing this branch onto its release commit.

The SDKs are LiveKit's, under the Apache-2.0 licence in `LICENSE`. This is
Rift's fork, not an official LiveKit release, and LiveKit has not reviewed or
endorsed it. Each changed file marks what changed with `RIFT PATCH`.
