# image-viewer

Show an image, fit to the screen.

**Built, host-tested.** The logic (the fit-to-screen blit) has a host self-test - `make check` - and compiles
freestanding for the target - `make skeleton`. The on-console payload (the display loop, the
file read and decode) is written the way [gallery](../gallery/) shows, and links the moment the SDK's
`oops/freestd.h` is populated - the base-layer promotion left that header empty, which blocks
every payload elf in the collection today, porthole's included.
