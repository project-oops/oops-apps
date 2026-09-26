# Acknowledgements

oops-apps is a clean-room implementation. It contains no code from any of the projects
below. They are credited as **references that pointed at a capability or documented an
interface** - the direction, never the source.

## Platform Authentication and Entitlement Bypasses

The `pltauth-patch` payload addresses the platform authentication daemon kill (`0x80de0051`)
observed when launching native PS5 title directories without a retail PSN ticket.
Credit is extended to the PS5 research community for isolating the interaction between
Sony's userland `PFAuthClient` and the `/dev/pltauth` character device interface:

- **etaHEN / LightningMods** - for researching and demonstrating the role of `/dev/pltauth`
  ioctls in PlayStation platform authentication checks across firmware versions.
- **ps5-kstuff / sleirsgoevy / ChendoChap** - for pioneering userland/kernel R/W payload
  chains and reverse-engineering the FreeBSD device switch (`cdevsw`) dispatch architecture
  on Prospero.

## Decoupled Sandbox Namespace Service

The `sandbox-daemon` application follows architectural patterns from open-source console
research projects. These projects are credited for the credential / vnode manipulation
approach and the FreeBSD kernel structure layouts.

The original trigger-file polling mechanism (`/download0/etahen_jailbreak`) was replaced
with a direct loopback TCP IPC service (`127.0.0.1:9069`) to address Prospero FW 12.40
constraints where native Big App sandboxes do not mount `/download0`. Dynamic `allproc`
discovery via kernel memory scanning (adapted from `pltauth-patch`) replaced static
heuristic offsets for reliable process resolution.

- **LightningMods/etaHEN** (`https://github.com/LightningMods/etaHEN`) - the original
  trigger-file mechanism and credential elevation pattern. The loopback IPC service
  supersedes the file-based handshake for FW 12.40+ compatibility.
- **ArkSama/PS5-Lapy-JB-Daemon** (`https://github.com/ArkSama/PS5-Lapy-JB-Daemon`) -
  the clean-room standalone breakout for consoles running `kstuff`. Shows the minimal
  PID-parse, descriptor-update, and trigger-deletion pattern.
- **pltauth-patch** (`oops-apps/src/pltauth-patch`) - dynamic allproc discovery via
  `[kdata+0x2600000, kdata+0x2B00000]` kernel memory scanning, confirmed working on
  FW 12.40.
- **FreeBSD kernel** (`sys/sys/proc.h`, `sys/sys/ucred.h`, `sys/sys/jail.h`) - the structure
  layout offsets used in the credential elevation and vnode redirection.
