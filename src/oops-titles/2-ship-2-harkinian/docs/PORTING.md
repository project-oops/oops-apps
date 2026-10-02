# Porting notes

How this title is built, and where its data comes from.

## Two archives, and only one of them is yours to supply

libultraship reads its resources out of `.o2r` archives, and this port needs two of them
for different reasons.

`2ship.o2r` is the **port's own assets** - its fonts, its UI textures, and libultraship's
shaders. It contains nothing from a cartridge, and it is generated without a ROM:
upstream has a target for exactly this, whose whole purpose is the no-ROM half.
The official release asset ships in `prebuilt/2ship.o2r` and `make package` stages
it beside the payload. Without it libultraship refuses to start, which is why a package
missing it never reaches a menu.

`mm.o2r` is **converted from a Majora's Mask ROM the player owns**, and this build
never asks for one. The dependency is a runtime question: the title looks for the
archive under `OOPS_POSIX_HOME` when it starts, and if a cartridge ROM (.z64, .n64, .v64)
is placed beside eboot.bin, the payload runs the embedded ZAPD and OTRExporter to convert
it on first launch. `shim/2s2h_start.c` reports it before libultraship looks, so the answer
is in the log as well as on screen.

## Targets

```
make SOH_ARMED=1            # the payload
make SOH_ARMED=1 title      # the title package: the payload and its sce_sys
make SOH_ARMED=1 package    # the same with 2ship.o2r and assets.zip staged
make survey                 # what upstream provides and what this port answers
make compile-survey         # compile every source for the target, without linking
```

The payload is behind `SOH_ARMED`. Unarmed, `title` and `package` refuse rather than
repackaging whatever an earlier build left in `build/`.

## Notes

`OOPS_POSIX_HOME` is `/app0`. The title's home and its archives are both under the package.

The entry point is `shim/2s2h_start.c` (`two_ship_two_harkinian_start`). It walks `.init_array`
itself, because the loader does not, and libc++ and libultraship both build dispatch tables
there; it seeds `rand` from the clock, because `std::random_device` has no entropy source
on this platform.
