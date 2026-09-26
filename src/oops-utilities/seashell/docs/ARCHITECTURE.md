# SeaShell - architecture

The split that lets one codebase be both the Orbistoun front-end and a native shell on the
hardware. The [README](../README.md) is the overview.

## The model/host split

| | lives in | tested |
|---|---|---|
| navigation, screens, cursors, what each screen contains | `home.c` | with no display |
| drawing | `home.c` | into a plain memory surface (1280x720) |
| launching a title, installing a package, running a payload, power | **the host** | with a recorder |
| the display, the pad, the loop | `home_main.c` | on the hardware and in the emulator |

The model never launches anything directly. It hands the host an **action and an argument** and
the host decides what that means:

- **Orbistoun** maps the dispatch into emulator syscalls, process lifecycles, and save states.
- **The hardware** maps the dispatch into platform services and kernel calls.
- **Self-tests** map the dispatch into an action recorder to assert exact parameter passing.

**A host that declines is recorded, not swallowed.** `perform` returns whether it handled the
action, and the screen displays `NOT AVAILABLE HERE` when unhandled.

## Two targets, one codebase

1. **The front-end shell for Orbistoun** - drives emulator processes, save states, captures, and
   hardware configuration.
2. **A native shell on the hardware** - ships as a native eboot Big App (category 0, root
   privilege), installed to `/user/app/SCSH00001`; a plain-ELF build is also produced for the
   `elfldr` / Orbistoun path.

The split isolates UI state and software-canvas rendering from machine-specific execution.

## Persistence

Settings and favourites are serialised as a clean-room binary struct to
`/data/homebrew/SCSH00001/settings.bin` (with a fallback path), validating a magic header and
format version before applying. `oops_fs_read_all()` / `oops_fs_write_all()` are the freestanding
I/O underneath.

## Build outputs

Built under the strict warning set, across the project-wide four axes
(`OOPS/docs/CONVENTIONS.md` section 2):

- `dist/seashell-prospero.elf` - freestanding ELF for Orbistoun and `elfldr`.
- `dist/seashell-eboot-prospero.bin` - fake-signed executable container (`fSELF`).
- `dist/seashell-title-prospero.zip` - native Big App category 0 bundle for
  `/user/app/SCSH00001`.
