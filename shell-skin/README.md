# shell-skin

Restyle the home screen by injecting a userscript into it - a background, a corner mark, and
(once its selector is known) hiding the store tile.

## Why it is shaped as a daemon, not a file edit

The system UI, **SceShellUI**, is a WebKit process, so a modification is CSS and JavaScript - a
userscript, exactly as a browser extension applies one. What it is *not* is a file you edit: the
shell's assets live on a **read-only, integrity-verified system partition** that a system update
reverts, so editing them on disk is neither persistent nor safe.

So the model is a **background service that injects at boot and re-applies after a shell
restart**. Nothing is written to the protected partition; the change is re-asserted each time
SceShellUI comes up. The composed script is idempotent by construction - every element it adds
is guarded by an id and skipped if already present - which is exactly what makes re-applying
safe.

The pieces to run this already exist in the collection: the injector takes over a native process
(SceShellUI is one), `pros restart-ui` detects and respawns the shell, and Prosperous keeps a
service alive.

## What is real here, and what is gated

- **The content is real and tested.** `shell_skin_compose` builds the userscript; `make check`
  verifies each recipe is present, the script is idempotent, and it refuses to truncate.
- **The delivery is unproven.** The one hard step - reaching the running SceShellUI WebKit
  context to *evaluate* the script - is not demonstrated, which is why there is no `elf` target.
  It gates on an obSCEne probe: *can a payload inject into SceShellUI and reach its JS context.*
  Same discipline as the PS2 work - measure the capability on hardware before building on it.

Honesty about one recipe: **BACKGROUND** and **BANNER** overlay layers of their own and need to
know nothing about the shell's markup, so they are robust. **HIDE_STORE** targets an element by
selector, and that selector is a **hypothesis** until the real DOM is inspected on hardware - it
is shipped as a placeholder that does nothing, not a guess that might hide the wrong thing.

## Building

```bash
make check      # compose the userscript and verify it - runs anywhere
make skeleton   # the compose logic compiles freestanding for the target
```
