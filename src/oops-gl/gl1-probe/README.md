# gl1-probe

<p align="center">
  <img src="assets/logo.png" alt="gl1-probe" width="200">
</p>

The breadth probe for **OpenGL 1.x** on oops-gl.

## About

`gl1-cube` is a pinned oracle that guards one measured frame; gl1-probe is the other half. It
exercises the entry points the oracle never touches, one feature at a time.

- **Every check reads the pixels back and decides.** A GL call that returns without error proves
  nothing; the failure it looks for is a call that succeeds and draws the wrong thing.
- **Fixtures that can fail.** Each scene is skewed - no square symmetric about x=y, no colour with
  equal channels - so a broken implementation cannot pass by luck.
- **One suite, two targets.** It runs identically on the host software rasteriser and on the
  hardware, so a result that differs between them is a hardware-path bug.

## Screenshot

<p align="center">
  <img src="../../../common/assets/no-screenshot.svg" alt="No screenshot yet" width="600">
</p>

## Docs

- [oops-apps catalog and guide](../../../docs/USER_GUIDE.md)
