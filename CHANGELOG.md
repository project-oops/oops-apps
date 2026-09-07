# Changelog

oops-apps is a home for programs, not a single artifact - each app under it builds and ships
on its own. There is no repository-wide version; what a release means is decided per app, and
an app's own `README.md` is where its state is recorded.

Entries are grouped **Added / Changed / Fixed**, newest first.

Nothing has shipped yet - this is the initial state.

## [unreleased]

### Added

- **A repository for the homebrew this collection builds on its own SDK.** Something a person
  runs on the hardware for its own sake - a tool, a demo, a service - built on
  [oops-sdk](https://github.com/project-oops/oops-sdk). The admission rule is deliberately
  narrow: a probe that exists to *measure* the platform belongs in obSCEne, an app that *does*
  something belongs here. (D001)
- **Porthole, moving in from obSCEne.** The target half of the capture-and-input path whose
  host half lives in Prosperous - it was a payload inside a probe and is now an app in its own
  right, which is what prompted the repository. The freestanding base layer it needs came in
  with it on loan, and belongs in oops-sdk rather than here. (D002)

### Changed

- **Porthole's encoder session is off unless a build asks for it.** The four struct-taking
  encoder calls pass parameter layouts not yet confirmed against the platform, which obSCEne's
  D300 had reserved for M2, yet every start made two of them and every frame the other two. They
  are now compiled in only with `PORTHOLE_ENCODER_SESSION`, a default build serves the template
  stream, and the selftest asserts the default. (D003)

### Fixed

- **Porthole's serving loop no longer waits on its input socket.** A receive that blocked held
  the video frame with it, so the stream stalled whenever a pad was at rest and the host went
  quiet; a blocking accept meant no video at all until an input client also connected. The
  listeners are now non-blocking, the input receive takes only what has arrived, and the video
  connection is set to block explicitly.
- **A host that reconnects is a new sender.** Each slot's last sequence is forgotten when a new
  input connection is accepted, so a restarted host's records are not dropped as stale against
  the previous sender's high mark. A superseded record now reports `PORTHOLE_STALE` rather
  than looking applied, which is what lets the selftest tell the two apart.
