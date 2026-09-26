# net-tool

<p align="center">
  <img src="../../../common/assets/no-logo.svg" alt="No logo yet" width="200">
</p>

A network status panel and a socket to talk to, built on the SDK's inet helpers.

## About

net-tool shows what the network is - link state, addresses - and stands up a small UDP responder
reachable from another machine, so the platform's networking can be checked end to end.

- **Two halves, both tested.** The panel and the inet helpers have a host self-test
  (`make check`); the display loop and UDP status responder package as a conforming release
  artifact.
- **A reachability panel, not a verdict.** It reports what the SDK maps and answers; confirming
  the stack's behaviour on the hardware is obSCEne's job.

## Screenshot

<p align="center">
  <img src="../../../common/assets/no-screenshot.svg" alt="No screenshot yet" width="600">
</p>

## Docs

- [oops-apps catalog and guide](../../../docs/USER_GUIDE.md)
