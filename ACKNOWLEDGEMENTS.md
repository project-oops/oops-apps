# Acknowledgements

oops-apps is a clean-room implementation. It contains no code from any of the projects
below. They are credited as **references that pointed at a capability or documented an
interface** — the direction, never the source.

## Platform Authentication and Entitlement Bypasses

The `pltauth-patch` payload addresses the platform authentication daemon kill (`0x80de0051`)
observed when launching native PS5 title directories without a retail PSN ticket.
Credit is extended to the PS5 research community for isolating the interaction between
Sony's userland `PFAuthClient` and the `/dev/pltauth` character device interface:

- **etaHEN / LightningMods** — for researching and demonstrating the role of `/dev/pltauth`
  ioctls in PlayStation platform authentication checks across firmware versions.
- **ps5-kstuff / sleirsgoevy / ChendoChap** — for pioneering userland/kernel R/W payload
  chains and reverse-engineering the FreeBSD device switch (`cdevsw`) dispatch architecture
  on Prospero.

