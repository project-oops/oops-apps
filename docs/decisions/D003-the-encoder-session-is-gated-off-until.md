# D003 - The encoder session is gated off until its parameter layouts are known


**decided** · 2026-09-07

Porthole has never run on a console from this collection: the only hardware-measured
milestone is M0, the encoder sysmodule load. The next run should answer one question at a
time, and the code as it stood would have answered none, because it crashed first.

obSCEne's D300 settled the symbol resolution and, in the same breath, reserved the
**struct-taking** encoder calls for M2: `sceVencCoreQueryMemorySize`, `CreateEncoder`,
`SetInputFrame` and `GetAuData` each take a parameter layout this payload has not confirmed
against the platform, and a wrong layout is stack corruption in a process holding kernel
read/write. Yet `porthole_encoder_open` had grown to end by creating a session - passing our
own `porthole_encoder_config` to two of those calls on every start - the entry point retried
it explicitly, and the capture path called the other two into a four-kilobyte stack buffer
every frame. Any of them failing would have come before a socket opened, so a run that
crashed would have said nothing about the sockets, the template stream, or input.

So the four calls are compiled in only when `PORTHOLE_ENCODER_SESSION` is defined, which
`make elf PORTHOLE_ENCODER_SESSION=1` does and nothing else does. A payload built without it
loads the encoder module, resolves its symbols, opens both sockets, serves the template stream
and applies input - everything that is measured or that fails visibly in the klog, and nothing
that could corrupt the stack. `porthole_encoder_session_create` and `porthole_encoder_query_memory`
also refuse with `PORTHOLE_UNIMPLEMENTED` in a gated build, so a caller added later cannot
reach the calls by another route; and the host selftest asserts through
`porthole_encoder_session_enabled` that the default is the gate, so a build that turned it on
by accident fails `make check` rather than reaching hardware.

**What turns it on:** the M2 work as written in the README - confirming the parameter
structures from public toolchain sources - and then a build with the define. The gate is a
build flag rather than a runtime switch on purpose: a switch that could be flipped from a
socket would put the unconfirmed calls one packet away from the first run this exists to
protect.
