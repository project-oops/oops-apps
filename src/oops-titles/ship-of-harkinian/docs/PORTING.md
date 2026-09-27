# Porting notes

How this title is built, and where its data comes from.

## Two archives, and only one of them is yours to supply

libultraship reads its resources out of `.o2r` archives, and this port needs two of them
for different reasons.

`soh.o2r` is the **port's own assets** - its fonts, its UI textures, and libultraship's
shaders. It contains nothing from a cartridge, and it is generated with
`extract_assets.py --norom`: upstream has a target for exactly this, `GenerateSohOtr`,
whose whole purpose is the no-ROM half. The build produces it and `make package` stages
it beside the payload. Without it libultraship refuses to start, which is why a package
missing it never reaches a menu.

`oot.o2r` (and `oot-mq.o2r` for Master Quest) is **converted from a ROM the player
owns**, and this build never asks for one. The dependency is a runtime question: the
title looks for the archive under `OOPS_POSIX_HOME` when it starts, and says what is
missing and where to put it when it is not there. `shim/soh_start.c` reports it before
libultraship looks, so the answer is in the log as well as on screen.

## Generating `soh.o2r`

The converter is a **host** tool, not payload code. `extract_assets.py` drives ZAPD,
ZAPD links OTRExporter whole-archive, and OTRExporter pulls libultraship, so producing a
4MB archive of fonts wants a desktop build of the engine. `shim/Extract.cpp` exists
because that toolchain is not in the payload: it answers the seven methods the game
calls, and says what is missing rather than reporting a conversion it did not do.

The host build is done in a container, and the specifics matter:

- **Ubuntu 22.04.** libultraship's logging is written against the `fmt` that ships there
  (8.1.1). A newer distribution fails to compile `gfx_sdl2.cpp` on a `consteval` format
  string, and `find_package(spdlog REQUIRED)` has no vendored fallback to retreat to.
- **CMake 3.26 or newer**, which 22.04 does not have; install it with pip. Pin it below
  4, because CMake 4 drops the compatibility these package configs rely on.
- **The apt list upstream's own CI uses**, `.github/workflows/apt-deps.txt`, plus
  `libzip-dev`, `lsb-release` and `git`.
- **A CMake package config for tinyxml2.** 22.04's `libtinyxml2-dev` installs the
  library and headers and no config file, so `find_package(tinyxml2 REQUIRED)` cannot
  succeed against it. The library is there; only the description of it is missing, so
  write a `tinyxml2Config.cmake` naming `IMPORTED_LOCATION` and `INTERFACE_INCLUDE_DIRECTORIES`
  and point `-Dtinyxml2_DIR` at it. Put it somewhere that outlives the container, or the
  next configure reads it out of the cache and cannot find it.

Then `cmake --build <build> --target GenerateSohOtr`, and copy the result to
`build/soh.o2r`, which is where `make package` looks.

## Targets

```
make SOH_ARMED=1            # the payload
make SOH_ARMED=1 title      # the title package: the payload and its sce_sys
make SOH_ARMED=1 package    # the same with soh.o2r staged, which is what runs
make survey                 # what upstream provides and what this port answers
make compile-survey         # compile every source for the target, without linking
```

The payload is behind `SOH_ARMED` because two dependencies are unvendored. Unarmed,
`title` and `package` refuse rather than repackaging whatever an earlier build left in
`build/`, which would otherwise reach the console looking current.

## Notes

`OOPS_POSIX_HOME` is `/app0`. `/data` is outside the app sandbox and writing there
unmounts `/app0` and every asset in it, so the title's home and its archives are both
under the package.

The entry point is `shim/soh_start.c`. It walks `.init_array` itself, because the loader
does not, and libc++ and libultraship both build dispatch tables there; it seeds `rand`
from the clock, because `std::random_device` has no entropy source on this platform and
unseeded it is a fixed sequence.
