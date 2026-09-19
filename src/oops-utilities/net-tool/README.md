# net-tool

What the network is, and a socket you can talk to.

**Built, host-tested, and packaged.** The logic (the panel and the SDK inet helpers) has a host self-test - `make check` - and compiles
freestanding for the target - `make skeleton` and `make elf`. The on-console payload (the display loop, the
UDP status responder, and link status) stages as a conforming four-axis release artifact (`net-tool-prospero.elf`).
