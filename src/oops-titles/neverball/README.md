# Neverball

<p align="center">
  <img src="assets/logo.png" alt="Neverball" width="200">
</p>

Tilt the floor, not the ball — an open-source arcade balance game, ported to the console.

## About

Neverball rolls a ball through an obstacle course by tilting the floor, racing the clock and
collecting coins. It is one of the collection's ported upstream titles, tracked against a pinned
release.

- **Pinned to a finished release** — `neverball-1.6.0`, by commit hash rather than tag, so the
  tree a build starts from can't move underneath it.
- **Fetched, not vendored** — the upstream sources are pulled on demand via `upstream.lock`; only
  the port's own `patches/` live here.

## Screenshot

<p align="center">
  <img src="assets/demo.gif" alt="Neverball running on Prospero, captured over JetKVM" width="600">
</p>

## Docs

- [oops-titles overview](../README.md)
- [oops-apps catalog & guide](../../../docs/USER_GUIDE.md)

---

<sub>Logo: the Neverball ball, from the upstream project's own icon, recomposited on black. Neverball is GPL v2.</sub>
