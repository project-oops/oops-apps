# NetSurf

<p align="center">
  <img src="assets/icon0.png" alt="NetSurf" width="200">
</p>

A small, fast web browser with its own layout engine, ported to the hardware.

## About

NetSurf is a browser in C with its own HTML and CSS engine, built here with its framebuffer
front end, which draws every pixel itself. JavaScript runs in Duktape, an interpreter, so
nothing needs executable memory.

- **Pinned to a finished release** - `release/3.11`, by commit hash.
- **Fetched, not vendored** - the browser is pulled on demand via `upstream.lock`; its parser
  and decoder libraries, each a repository of its own, are vendored under `src/oops-deps/`.
- **TLS is part of the port** - cURL fetches over a TLS library, without which most of the web
  does not load.

## Building

`make` prints the survey: the pin and the libraries still to vendor.
`make NSRF_ARMED=1 title` builds the payload.

## Docs

- [oops-titles overview](../README.md)
- [oops-apps catalog and guide](../../../docs/USER_GUIDE.md)
