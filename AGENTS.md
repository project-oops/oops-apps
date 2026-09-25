# AGENTS.md - oops-apps

Rules for agents working on an app, a title, a dependency or a shim here. Read
[`../AGENTS.md`](../AGENTS.md), [`../docs/CONVENTIONS.md`](../docs/CONVENTIONS.md) and
[`../docs/STYLE.md`](../docs/STYLE.md) first; this file only adds to them.

## The four rules

1. **Go through the SDK.** A title reaches the hardware through oops-sdk. A facility a port
   needs (storage, input, logging, a dialog, a clock) comes from the SDK, and goes into the SDK
   when it is missing.
2. **Check the SDK before reimplementing.** Read
   [`../oops-sdk/docs/API_INDEX.md`](../oops-sdk/docs/API_INDEX.md) first; the index finds a
   concept whose name you do not know, grep only a name you already guessed.
3. **A shim is minimal and belongs to one title.** `shim/` holds only what is true of that
   title. Anything a second port would want is an SDK entry point or lives in
   [`common/`](common/).
4. **A patch is minimal and belongs to one title.** `patches/` changes upstream only where
   upstream is wrong for this target. A patch that adapts the platform rather than the program
   is the platform's job, because a patch is carried and rebased indefinitely.

## Where a fix goes

Anything reusable goes in the SDK, anything non-reusable into the shim, anything game-specific
into a patch. Ask "who else would want this fix", not "which title found the bug".

| Fix | Where |
|---|---|
| a home directory that must exist before a program writes | SDK or `common/` |
| naming the file and errno of a failed `open` | SDK |
| an upstream tree fetched byte-for-byte as published | `common/` |
| `-DOS_LINUX=1`, an entry point, forwarding headers | that title's `shim/` |
| a hard-coded `main`, a path only this program builds | that title's `patches/` |

Before writing anything into a title, ask what the next port would have to copy. If the answer
is "this code", it belongs in the SDK or `common/`.

## App structure

- Every app includes `common/app.mk`.
- An app's entry file is named `<app>_main.c`.
- A helper used by more than one app goes in `common/` or oops-sdk, never copied between apps.
  Number formatting uses `oops_snprintf`.
- An app has one logger, `oops_log_*` with an app tag. No local `klog`, `say` or similar
  wrappers.
- The shim and patch policy for ported titles is stated once, in
  [`src/oops-titles/README.md`](src/oops-titles/README.md), not in per-title READMEs.
- A title with no code has no directory and no docs of its own.

## Telemetry

- Diagnostics (counters, log channels, runtime verbosity) go in oops-sdk or `common/` shims,
  never in one title.
- Log levels are set at run time by `/app0/oops-log`, one `channel=level` per line
  (`oops_log_channel_level`). Titles need no code for it.
- Check what is already measured before adding a counter. oops-gl counts triangles, draw calls,
  GPU wait, shader patching and command-buffer time per flip, printed when `/app0/oops-log`
  carries `gl=debug`.

## Build and verify

`<OOPS>` is the collection checkout. Build one app in the pinned container:

```bash
MSYS_NO_PATHCONV=1 docker run --rm -v "<OOPS>:/w" \
    -w /w/oops-apps/src/<category>/<app> silkeh/clang:21 make all
```

- `MSYS_NO_PATHCONV=1` stops Git Bash rewriting `/w` into a Windows path.
- A build failing on paths under `/mnt/c/...` is stale dependency output from a WSL build:
  `rm -rf build` in that app and rebuild.
- `./bin/oops-apps list | build | check | dist` reach every app the way CI does.
- `./bin/oops-apps check` is the gate. There is no root `make check`.
