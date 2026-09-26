# D004 - Porthole captures and serves; it does not encode or apply input

**Status:** decided
**Date:** 2026-09-26

Porthole is a capture-and-serve payload: it reads the scanout surface through kernel
read/write and serves it over its own sockets. It does not encode in hardware, and it receives,
checks and sequence-tracks controller records without applying them.

**Why:** from an unsigned payload, `sceSysmoduleLoadModule` refuses the video encoder module
(`0xa0020101`) and the pad library (`0x805a1000`), and no `sceVencCore`, `scePad` or
`sceVideoOut` entry point resolves by dlsym, the export table or the dynamic-library walk. The
pad virtual-device functions resolve to zero in a packaged title as well. Sockets through the
SDK's POSIX layer work from a payload, and `/dev/dce` opens and names the scanout buffers.

**Rejected:**
- Hardware encode: the module does not load for a payload.
- Pad injection: the virtual-device interface is exported to neither a payload nor a title.
- Delivering Porthole as a title: it must run beside another title as a resident service.
