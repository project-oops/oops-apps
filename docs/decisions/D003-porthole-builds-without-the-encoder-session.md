# D003 - Porthole builds without the encoder session

**Status:** decided
**Date:** 2026-09-26

The struct-taking encoder calls (`sceVencCoreQueryMemorySize`, `CreateEncoder`,
`SetInputFrame`, `GetAuData`) compile only when `PORTHOLE_ENCODER_SESSION` is defined, which
`make elf PORTHOLE_ENCODER_SESSION=1` does and nothing else does. Without it the encoder is
attempted and never required: the video path is the template stream, and the session entry
points refuse with `PORTHOLE_UNIMPLEMENTED`.

**Why:** the calls take parameter layouts this payload has not confirmed, and a wrong layout
is stack corruption in a process holding kernel read/write. The encoder module does not load
for an unsigned payload (D004), so a build that required it would never open a socket. The
host self-test asserts the default is the gated build.

**Rejected:**
- A runtime switch: it would put unconfirmed calls one packet away from any run.
- Requiring the encoder before serving: the payload would exit before its sockets open.
