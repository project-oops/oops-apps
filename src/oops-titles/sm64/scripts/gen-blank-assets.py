#!/usr/bin/env python3
"""Write blank stand-ins for upstream's ROM-derived assets, so a build needs no ROM.

upstream/assets.json carries every asset's byte size, its position in the ROM, and for a texture
its pixel dimensions. That is enough to create a correctly sized empty file for each one. The
game's addresses and offsets come out identical to a ROM build; only the contents are zero, and
those are filled in at run time from a ROM the player supplies.

Two outputs per texture, because two consumers want different shapes:

  <upstream>/<asset>.png              the source file upstream's own rules convert
  <build-dir>/<asset>.inc.c           the byte list the game's C sources #include directly

The .inc.c files are written here rather than left to upstream's rules so that a build needs
neither a ROM nor upstream's host tools.

Sequences and raw binaries get a zero file at their own path. The sound banks and their samples
are not covered: upstream carves those with tools/disassemble_sound.py and rebuilds them
host-native with tools/assemble_sound.py, so they are not a slice of the ROM and a zero file of
the right length does not stand in for one.

  gen-blank-assets.py <upstream-dir> <build-dir>
"""

import json
import os
import struct
import sys
import zlib


def png_chunk(tag, payload):
    body = tag + payload
    return struct.pack(">I", len(payload)) + body + struct.pack(">I", zlib.crc32(body))


def write_blank_png(path, width, height):
    """An 8-bit RGBA image of zeroes. n64graphics reads the dimensions from here and re-encodes to
    whatever format the file name names, so the colour type only has to be one it accepts."""
    raw = b"".join(b"\0" + b"\0" * (width * 4) for _ in range(height))
    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(png_chunk(b"IHDR", header))
        f.write(png_chunk(b"IDAT", zlib.compress(raw, 9)))
        f.write(png_chunk(b"IEND", b""))


def write_blank_inc_c(path, size):
    """The same shape upstream's hexdump rule produces: a comma-separated byte list, no braces."""
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="ascii") as f:
        f.write("0x00," * size)
        f.write("\n")


# tools/skyconv.c restructures rather than slices, so these four assets need C declarations instead
# of a byte list. Its IMAGE_PROPERTIES and TABLE_DIMENSIONS give the geometry, and the sizes in
# assets.json agree: cake is 4x12 tiles of 80x20 at 2 bytes a pixel, which is its 153600 exactly,
# and cake_eu 5x7 of 64x32, which is its 143360.
#
# A skybox is 8x8 tiles of 32x32, and its ROM size counts only the tiles that are not duplicates
# plus an 80-entry pointer table. A blank build has no duplicates to find, so all 64 are emitted and
# the table addresses them the way skyconv's own loop does, row * 8 + col % 8.
CAKE_TILES = {"cake": (4 * 12, 80 * 20 * 2), "cake_eu": (5 * 7, 64 * 32 * 2)}
SKYBOX_TILE_BYTES = 32 * 32 * 2
SKYBOX_TILES = 8 * 8


def write_blank_cake(path, name):
    count, tile_bytes = CAKE_TILES[name]
    suffix = "eu_" if name.endswith("_eu") else ""
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="ascii") as f:
        for i in range(count):
            f.write(f"ALIGNED8 static const Texture cake_end_texture_{suffix}{i}[] = {{\n")
            f.write("0x00," * tile_bytes)
            f.write("\n};\n\n")


def write_blank_skybox(path, name):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="ascii") as f:
        f.write('#include "types.h"\n\n#include "make_const_nonconst.h"\n\n')
        for i in range(SKYBOX_TILES):
            f.write(
                f"ALIGNED8 static const Texture {name}_skybox_texture_"
                f"{i * SKYBOX_TILE_BYTES:05X}[] = {{\n"
            )
            f.write("0x00," * SKYBOX_TILE_BYTES)
            f.write("\n};\n\n")
        f.write(f"const Texture *const {name}_skybox_ptrlist[] = {{\n")
        for row in range(8):
            for col in range(10):
                tile = row * 8 + (col % 8)
                f.write(f"{name}_skybox_texture_{tile * SKYBOX_TILE_BYTES:05X},\n")
        f.write("};\n\n")


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    up, build = sys.argv[1], sys.argv[2]
    with open(os.path.join(up, "assets.json"), encoding="utf-8") as f:
        assets = json.load(f)

    made = {"png": 0, "m64": 0, "bin": 0}
    inc_c = 0
    skyconv = 0
    skipped_sound = 0
    no_dimensions = []

    for name, data in assets.items():
        if name.startswith("@"):
            skipped_sound += 1
            continue
        meta, size = data[:-2], data[-2]
        kind = name.rsplit(".", 1)[-1]

        if kind == "aiff":
            skipped_sound += 1
            continue
        if kind not in made:
            sys.exit(f"gen-blank-assets: {name}: unhandled kind {kind!r}")

        path = os.path.join(up, name)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        if kind == "png":
            stem = name[: -len(".png")]
            base = os.path.basename(stem)
            if stem.startswith("textures/skyboxes/"):
                write_blank_skybox(os.path.join(build, "bin", base + "_skybox.c"), base)
                skyconv += 1
                continue
            if base in CAKE_TILES:
                write_blank_cake(os.path.join(up, stem + ".inc.c"), base)
                skyconv += 1
                continue
            # The byte list only needs the size, which every entry carries.
            write_blank_inc_c(os.path.join(build, stem + ".inc.c"), size)
            inc_c += 1
            if len(meta) < 2:
                no_dimensions.append(name)
                continue
            write_blank_png(path, meta[0], meta[1])
        else:
            with open(path, "wb") as f:
                f.write(b"\0" * size)
        made[kind] += 1

    # sound/sound_data.c wraps four byte lists in `unsigned char x[] = { ... }`, so a blank of any
    # length links. The lengths here are the ROM's, from assets.json where it has them. They are
    # placeholders in the strongest sense: upstream does not slice these out of the ROM but rebuilds
    # them host-native, and the rebuilt ctl is larger than the ROM's, so a run-time path filling
    # these will size them for itself rather than trust these numbers.
    sound = {
        "sound_data.ctl.inc.c": assets.get("@sound ctl us", [0, 0])[-2],
        "sound_data.tbl.inc.c": assets.get("@sound tbl us", [0, 0])[-2],
        "sequences.bin.inc.c": 1,
        "bank_sets.inc.c": 1,
    }
    for leaf, size in sound.items():
        write_blank_inc_c(os.path.join(build, "sound", leaf), max(size, 1))

    for kind, n in made.items():
        print(f"  {kind:<4} {n:>5}")
    print(f"  sound placeholders {len(sound)}")
    print(f"  inc.c {inc_c:>5}")
    print(f"  skyconv-shaped {skyconv:>3}  (skybox .c and cake .inc.c, as C declarations)")
    print(f"  sound entries left to upstream's own pipeline: {skipped_sound}")
    if no_dimensions:
        print(f"  no pixel dimensions, not written: {len(no_dimensions)}")


if __name__ == "__main__":
    main()
