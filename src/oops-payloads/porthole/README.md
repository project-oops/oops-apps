# Porthole

<p align="center">
  <img src="../../../common/assets/no-logo.svg" alt="No logo yet" width="200">
</p>

A freestanding, zero-dependency remote-play payload for a jailbroken Prospero-generation console.

## About

Porthole streams the console's own video out and takes controller state in, talking directly to PC
host tooling (`pros-core::watch` in [Prosperous](../../../../prosperous/)) without speaking the
vendor's Remote Play protocol.

- **The protocol is a design decision, not a reverse-engineering problem.** Because the console
  runs our code, there's no PIN pairing, no per-session crypto handshake, and no third-party client
  dependency — two fixed TCP sockets, raw H.264 out and a 24-byte pad record in.
- **Strict freestanding C** — no libc, no heap, no floats or variadics; it resolves its encoder
  entry points by walking the live kernel export table.
- **Served without waiting** — video flows with no input client connected and keeps flowing while
  the pad is at rest.

## Screenshot

<p align="center">
  <img src="../../../common/assets/no-screenshot.svg" alt="No screenshot yet" width="600">
</p>

## Docs

- **[Reference](docs/REFERENCE.md)** — the wire protocol, the build, and milestone status.
- [Design spec: prosperous/docs/VIDEO.md](../../../../prosperous/docs/VIDEO.md) — "Part three: Porthole".
- [oops-apps catalog & guide](../../../docs/USER_GUIDE.md)
