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
