# tracer

<p align="center">
  <img src="../../../common/assets/no-logo.svg" alt="No logo yet" width="200">
</p>

An in-process API and GPU-shader telemetry tracer for target processes.

## About

tracer runs inside a title (or as a standalone diagnostic payload) and records what really
happens: API calls with their arguments and returns, the layouts of out-parameters, and the GPU
command and shader streams the title submits.

- **The passive observation engine of THE LOOP.** It captures real call sequences, PM4 packets and
  RDNA2 shader bytecode from commercial titles on hardware — the ground truth that Orbistoun's
  decoders and recompiler are checked against.
- **Safe under load.** Per-NID call capping keeps a hot loop from disturbing frame pacing, and
  large buffers are hashed rather than copied, so no copyrighted asset leaves the console.
- **Offline decode.** The trace is a binary file pulled off the console and decoded host-side into
  standard `OBS|` records.

## Screenshot

<p align="center">
  <img src="../../../common/assets/no-screenshot.svg" alt="No screenshot yet" width="600">
</p>

## Docs

- **[Reference](docs/REFERENCE.md)** — capabilities, role in THE LOOP, and the offline decode path.
- [oops-apps catalog & guide](../../../docs/USER_GUIDE.md)
