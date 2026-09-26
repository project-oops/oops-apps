# pad-viz - reference

How pad-viz reads the controller, and where it sits relative to the probe line. The
[README](../README.md) is the overview.

## The batched low-latency read

pad-viz reads the pad with the SDK's **batched low-latency path** (`oops_input_poll_batch`) - one
driver request a frame returning up to a full batch of samples - and draws the newest, showing the
sample count so the read visibly delivers more than one record per frame. That path preserves a
press-and-release falling between two ordinary polls.

Every ordinary button is free to press and watch; **L1+R1+Options** together exits.

## The probe line

pad-viz is on the probe line (see
[D001](../../../../docs/decisions/D001-one-repository-for-apps-built-on-the-sdk.md)): it shows the
controller, a person judges whether it matches what they are pressing, and there is no verdict.
Confirming the input record's field layout is obSCEne's job (the input census); this draws
whatever the SDK maps.

## Build

The drawing (`pad-viz.c`) has a host self-test (`make check` renders neutral, all-pressed,
deflected and disconnected states into a buffer and checks bounds) and compiles freestanding
(`make skeleton`). The payload - `pad-viz_main.c`, the display and the batched read loop - links
for the hardware with `make elf`.
