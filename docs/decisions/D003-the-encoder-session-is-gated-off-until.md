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

---

**amended** · 2026-09-08

The gate did not do what this entry claimed for it, and hardware said so.

obSCEne's run of 2026-09-08 measured all three delivery routes, and no `sceVencCore` entry
point resolved on any of them. Not one of the twenty-four is in the export table an elfldr
payload is handed either, read directly out of that run's own dump. So `porthole_encoder_open`
fails on the hardware this is aimed at, on the route Porthole ships on - it has to be a
resident service, and a title cannot be one.

**A correction to an earlier draft of this entry.** It said `sceSysmoduleLoadModule` was not
available to a payload at all. That was wrong, and the mistake is worth keeping visible for its
shape: the probe's encoder section reported the loader absent, and the loader is in fact in the
payload's export table at a real address. The section had not looked there. An absence in a
report is a statement about where the report looked, and this entry repeated one as a fact.

That was fatal to the gate's purpose, because two places required the encoder before anything
else could happen: `porthole_run` returned immediately unless the open succeeded, and
`porthole_capture_encode` refused unless it had. A gated payload would therefore have logged
one line and exited **without opening a socket**, and the run designed to exercise the sockets
could never have reached them.

The encoder is now **attempted, never required**. Gated off, the video path is the template
stream, which needs no encoder at all: `porthole_run` logs the status and carries on, and
capture serves the template. A build *with* the gate on still refuses, because it was asked for
real video and has none to give, and a host build still refuses because it has no target to
serve and would bind real ports if it tried.

The host selftest now checks the template stream is well-formed Annex-B, opening on sequence
parameters, picture parameters and an IDR, since that is what a first hardware run puts on the
wire.
