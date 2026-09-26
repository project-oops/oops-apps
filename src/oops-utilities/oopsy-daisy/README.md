# OOPSy-DAISY

<p align="center">
  <img src="assets/logo.svg" alt="OOPSy-DAISY" width="200">
</p>

**OOPSy-DAISY**: **D**ownload **A**rtificial **I**ntelligence **S**lop **Y**ourself.

Browse the oops-apps catalogue and install homebrew **on the console itself** — pick a title,
and OOPSy-DAISY downloads and unpacks it into the homebrew folder. No PC, no staging tool, no
cable. This is the last link in the loop: everything else builds and publishes homebrew, and
this runs on the device and pulls it down.

## How it works

```
release (latest-main) ──► fetch the catalogue ──► pick a title ──► download its .zip ──► unpack into /data/homebrew ──► launch
```

- **One catalogue.** It reads the rolling `latest-main` release — the same source the
  [web index](https://project-oops.github.io/oops-apps/) shows — and lists the titles that ship
  a `.zip`. There is no second list to keep in step.
- **The install is a drop-in.** A packaged title `.zip` carries a top-level `<TITLE_ID>/`, so
  unpacking it into `/data/homebrew` lands it exactly where the console scans for homebrew.
- **It runs over the SDK.** The catalogue fetch and download use [`oops/http.h`](../../../../oops-sdk/docs/API_REFERENCE.md); the
  unpack uses [`oops/zip.h`](../../../../oops-sdk/docs/API_REFERENCE.md); the screen and pad use the display and input runtime.

## Status

The pure parts — parsing the catalogue, the install path, the screen — are done and host-tested
(`make check`). The on-device pipeline is written against the SDK's HTTPS client and zip
extractor; it needs the console's secure-transport module, which is pending the hardware target
coming back online. Until then OOPSy-DAISY builds as a plain payload (`FORMATS=elf`) and stays
out of the index; once it installs for real it ships as a title of its own — installable, of
course, from itself.

## Build & run

```bash
make check     # host self-test
make elf       # freestanding payload ELF for the console
```

Deploy the ELF with [Prosperous](../../../../prosperous/) like any other payload while it is in
bring-up.
