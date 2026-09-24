# oops-app-downloader

Browse the oops-apps catalogue and install homebrew **directly on the console** — no PC, no
staging tool, no cable. This is the last link in the loop: everything else builds and publishes
homebrew, and this runs on the device and pulls it down.

## What it is

A native on-device front end for [the oops-apps index](https://project-oops.github.io/oops-apps/).
It shows the same apps the web index shows, and for each one it can fetch that app's latest build
and unpack it into the console's homebrew folder, ready to launch — the way an app store on any
other device works, but for our own homebrew.

## Closing the loop

The collection already takes an app all the way from source to a downloadable build:

```
write app ─► oops-sdk / oops-mesa build ─► SELFish packages a title ─► release: <app>-title-<gen>.zip
```

Today a person finishes that journey at a PC: download the zip, unpack it, copy it over with
Prosperous. oops-app-downloader moves that last step onto the console itself:

```
              ┌─────────────────────── on the console ───────────────────────┐
release  ──►  │  fetch catalogue  ─►  pick an app  ─►  download its title.zip │
(latest-main) │        │                                        │             │
              │        ▼                                        ▼             │
              │  the same list the                     unpack into            │
              │  web index shows                       /data/homebrew/<id>    │
              └───────────────────────────────────────────────┬──────────────┘
                                                               ▼
                                                     launch from the dashboard
```

## Where the catalogue comes from

The index build publishes a machine-readable `apps.json` beside the web page
(`tools/build-apps-index.sh`), listing every app — name, title, kind, subtitle. The download URL
for each app is the release asset whose name starts with `<app>-` in the rolling `latest-main`
release, exactly as the web index resolves it. So this app reads one catalogue and one release,
and both are the same sources the website already uses — nothing here is a second list to keep in
step.

## What it needs from oops-sdk

The user interface, input, filesystem and networking sockets are all in oops-sdk today, and the
bring-up shell (below) uses them. Two facilities do not exist yet, and the real download path is
blocked on them. Both are filed on the oops-sdk request bus:

1. **An HTTPS client.** The catalogue and the release assets are served over HTTPS by GitHub, and
   oops-sdk exposes raw sockets only. The console's own secure-transport and HTTP system modules
   can do this; the SDK needs a small wrapper over them (an `oops/http.h`).
2. **On-device unpacking.** A title ships as a `.zip`, and the console has a compression system
   module the SDK can drive; the SDK needs a helper that extracts a zip to a directory
   (an `oops/zip.h`).

Both are reusable well beyond this app, which is why they belong in the SDK rather than here — the
same reasoning the collection applies to every shared facility.

## Status: bring-up shell

What builds today is a shell that opens the display, reads the pad, and shows which capabilities
are wired and which are still pending — an honest picture of the loop, not a fake front end over
functions that do not exist. When the two SDK facilities above land, the catalogue, download and
unpack steps drop into the places this shell already lays out, and `FORMATS` gains `title` so the
downloader itself becomes installable from the very index it serves.

- `oops-app-downloader.c` — the pure parts: the on-screen render, and the helper that turns a
  title id into its `/data/homebrew/<id>` install path. Both are host-testable.
- `oops-app-downloader_main.c` — the console entry: display + input loop.
- `oops-app-downloader_selftest.c` — the host test (`make check`).

## Build & run

```bash
make check     # host self-test (the pure parts)
make elf       # freestanding payload ELF for the console
```

Deploy the ELF with [Prosperous](../../../../prosperous/) like any other payload while it is in
bring-up. Once it installs titles for real it will ship as a title of its own.
