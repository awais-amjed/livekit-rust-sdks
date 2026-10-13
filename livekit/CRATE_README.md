# joinrift-livekit

LiveKit's Rust client SDK, release 0.9.5, with one change that
[Rift](https://joinrift.app) needs: **a two-channel audio track is published in
stereo.** The upstream SDK never lists `TF_STEREO` when it adds a track, so the
server answers mono and every track published from Rust is mono
([livekit/rust-sdks#1018](https://github.com/livekit/rust-sdks/issues/1018); the
same change as PR [#1023](https://github.com/livekit/rust-sdks/pull/1023)).
Everything else is upstream, unchanged, and the crate's other LiveKit
dependencies are LiveKit's own releases.

This is Rift's fork, not an official LiveKit release, and LiveKit has not
reviewed or endorsed it. It exists so the `rift-bot` crate can depend on it.
If you do not need stereo, use [`livekit`](https://crates.io/crates/livekit).

The library keeps the name `livekit`, so code written for the upstream crate
works unchanged:

```toml
[dependencies]
livekit = { package = "joinrift-livekit", version = "0.9.5" }
```

The source, and what changed, is in the `rift-crates` branch of
[awais-amjed/livekit-rust-sdks](https://github.com/awais-amjed/livekit-rust-sdks)
(`RIFT.md`; each changed file marks the change with `RIFT PATCH`). For how to use
the SDK, see [LiveKit's documentation](https://docs.livekit.io) and the
[upstream README](https://github.com/livekit/rust-sdks/tree/main/livekit).

Apache-2.0, as upstream.
