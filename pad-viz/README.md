# pad-viz

A live controller diagram. Buttons light when pressed, the stick thumbs move, the analog
triggers fill, the touch-pad shows its contacts, and a tilt box follows the accelerometer.
Every ordinary button is free to press and watch; **L1+R1+Options** together exits.

It reads the pad with the SDK's **batched low-latency path** (`oops_input_poll_batch`) - one
driver request a frame returning up to a full batch of samples - and draws the newest, showing
the sample count so the low-latency read is visibly delivering more than one record per frame.
That is the path that preserves a press-and-release falling between two ordinary polls.

**Built, host-tested.** The drawing (`pad-viz.c`) has a host self-test (`make check` renders
neutral, all-pressed, deflected, and disconnected states into a buffer and checks bounds) and
compiles freestanding (`make skeleton`). The on-console payload - `pad-viz_main.c`, the display
and the batched read loop - compiles freestanding; `make elf` links it for the console.

**On the probe line** (see [D001](../docs/decisions/D001-a-repository-for-apps-built-on-the-sdk.md)):
it shows the controller, a person judges whether it matches what they are pressing, and there is
no verdict. Confirming the input record's field layout is obSCEne's job (the input census); this
just draws whatever the SDK maps.
