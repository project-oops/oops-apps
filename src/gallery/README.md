# gallery

One page per SDK subsystem, driven by hand: a gradient and primitives, live pad state, what the
machine is, audio status, the network, and the capability matrix. L1/R1 to page, circle to exit.

The **capabilities** page shows what this console offers the SDK's media-decode and input-device
subsystems - video decode, audio decode and its AJM offload, keyboard, mouse, adaptive triggers -
straight from the matching `oops_*_available()` calls. It reports *reachability*: whether each
library and its entry points resolved here. That is state, the same kind the audio page's
open/closed and the net page's up/down already show - **not** a verdict on whether a full decode
or read works. Confirming behaviour is obSCEne's job (`107-videodec`, `108-audiodec`, and the
input-extension census), which is why the line below still holds.

**Built, host-tested.** Every page's drawing has a host self-test (`make check` renders all six
into a buffer and checks bounds, exercising both the available and absent branches of the
capability matrix) and compiles freestanding (`make skeleton`). The on-console payload -
`gallery_main.c`, the display and pad loop, which also gathers the capability matrix - is written
and compiles freestanding; `make elf` links it for the console.

**Closest to the probe line** (see [D001](../docs/decisions/D001-a-repository-for-apps-built-on-the-sdk.md)):
it shows, a person judges, there is no verdict. A pass/fail-per-subsystem version - one that calls
each function and rules on whether it *worked* - is a probe and belongs in obSCEne. The
capabilities page stays on the near side of that line by reporting only what resolved, never
whether it behaved.
