# net-tool

What the network is, and a socket you can talk to.

**Built, host-tested.** The logic (the panel and the SDK inet helpers) has a host self-test - `make check` - and compiles
freestanding for the target - `make skeleton`. The on-console payload (the display loop, the
echo server) is written the way [gallery](../gallery/) shows, and links the moment the SDK's
`oops/freestd.h` is populated - the base-layer promotion left that header empty, which blocks
every payload elf in the collection today, porthole's included.
