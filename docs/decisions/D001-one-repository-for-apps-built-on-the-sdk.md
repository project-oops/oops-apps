# D001 - One repository for apps built on the SDK

**Status:** decided
**Date:** 2026-09-26

oops-apps holds the programs the collection builds on oops-sdk: apps, titles, utilities and
developer payloads that a person runs on the hardware for what they do. A probe, whose purpose
is to measure what the platform does and report it, belongs in obSCEne instead.

**Why:** an SDK with one consumer is under-exercised, and a homebrew app otherwise has nowhere
to live but a repository of its own. Keeping measurement out keeps a probe's result from being
shipped as a feature.

**Rejected:**
- One repository per app: every app repeats the build, the gate and the release plumbing.
- Apps inside obSCEne: a measurement and a feature share a release and blur into each other.
- A fifth project: this is infrastructure under the four, like oops-libs and oops-sdk.
