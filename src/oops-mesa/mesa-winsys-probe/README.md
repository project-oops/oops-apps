# mesa-winsys-probe

<p align="center">
  <img src="assets/logo.png" alt="mesa-winsys-probe" width="200">
</p>

The smallest title that links upstream Mesa, and a report of how far radeonsi's startup gets.

## About

Every other check in oops-mesa is a compile. mesa-winsys-probe is a *link*: Mesa's 45 archives, the
winsys and runtime shims, the C++ support archive and oops-sdk in one binary, built the way a real
title builds. If the shape of the SDK fragment is wrong, this is what says so.

- **It walks radeonsi's init path** as far as it goes and logs where it stops. The whole chain
  from libdrm's first call to the end of `ac_query_gpu_info` has been traced on paper — but
  nothing in that prediction has met hardware yet.
- **The control, not the frontend.** It walks the winsys directly, so it stays the baseline that
  `dri-probe` (which brings up the frontend) is measured against.
- **Every answer the winsys gives is a value this collection decided** rather than read off the
  silicon — which is exactly what makes running it on hardware worthwhile.

## Screenshot

<p align="center">
  <img src="../../../common/assets/no-screenshot.svg" alt="No screenshot yet" width="600">
</p>

## Docs

- [oops-mesa bring-up docs](../../../../oops-mesa/docs/)
- [oops-apps catalog & guide](../../../docs/USER_GUIDE.md)
