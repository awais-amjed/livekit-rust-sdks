# `@joinrift/rtc-ffi-bindings`

LiveKit's `@livekit/rtc-ffi-bindings` 0.12.83, built from
[Rift](https://joinrift.app)'s fork of LiveKit's Rust SDKs with one change:
**a two-channel audio track is published in stereo.** The upstream SDK never
lists `TF_STEREO` when it adds a track, so every track published from Node is
mono ([livekit/rust-sdks#1018](https://github.com/livekit/rust-sdks/issues/1018)).

This is an internal package used by `@joinrift/rtc-node` to invoke the Rust
`livekit-ffi` bindings. Install that instead.

This is Rift's fork, not an official LiveKit release, and LiveKit has not
reviewed or endorsed it. The source is the `rift-npm` branch of
[awais-amjed/livekit-rust-sdks](https://github.com/awais-amjed/livekit-rust-sdks)
(`RIFT.md`). Apache-2.0, as upstream.
