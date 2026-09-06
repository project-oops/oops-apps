# system-panel

What the machine is, drawn to the screen.

The smallest app that proves the SDK reaches hardware end to end: it asks the system what it
is - generation, firmware, memory, user - draws the answer with the SDK's own text and shapes,
and waits for a button. Nothing platform-specific of its own; every line goes through
[oops-sdk](https://github.com/project-oops/oops-sdk).

## What it exercises

`oops_system_get_info` (system), `oops_draw_text` / `oops_draw_rect` / `oops_draw_clear`
(draw), and on hardware `oops_display_*` and `oops_input_*`. If this runs, the info path and
the drawing path both work on that machine - which is why it is the right thing to build
first on a firmware you have not tried.

## Building

```bash
make check      # render the panel into a host buffer and verify it drew - runs anywhere
make skeleton   # the drawing compiles freestanding for the target, object only
```

The render is deliberately split from where the pixels live: `system_panel_render` takes a
surface, so the same drawing runs against a display's framebuffer on hardware and against a
plain buffer in the host test. That seam is what makes it testable without a console, and it
is the SDK's own pattern.

## What is not here yet

The full on-console payload - a display opened, an input loop, an exit on circle - waits on
the base runtime (a crt0, a freestanding libc, syscalls) landing in oops-sdk. Every app here
waits on the same promotion; see [../docs/DECISIONS.md](../docs/DECISIONS.md) (D002). Until
then this builds as a host self-test and a freestanding skeleton, which is enough to prove the
drawing is sound and target-ready.

## The values on a host are not a machine's

Run on an ordinary computer, the platform calls behind `oops_system_get_info` are absent, so
the panel shows the SDK's defaults rather than real numbers. The host test checks that the
panel *drew*, never what it said - asserting on the values would be asserting on the host,
which measures nothing.
