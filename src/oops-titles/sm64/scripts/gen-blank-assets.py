#!/usr/bin/env python3
"""Write blank stand-ins for upstream's ROM-derived assets, so a build needs no ROM.

upstream/assets.json carries every asset's byte size, its position in the ROM, and for a texture
its pixel dimensions. That is enough to create a correctly shaped empty file for each one, which
upstream's own build rules then turn into correctly sized .inc.c arrays. The game's addresses and
offsets come out identical to a ROM build; only the contents are zero, and those are filled in at
run time from a ROM the player supplies.

Covers textures, sequences and raw binaries. The sound banks and their samples are not covered:
upstream carves those with tools/disassemble_sound.py and rebuilds them host-native with
tools/assemble_sound.py, so they are not a slice of the ROM and a zero file of the right length
does not stand in for one.

  gen-blank-assets.py <upstream-dir>
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
    """An 8-bit RGBA image of zeroes. n64graphics reads the dimensions from here and re-encodes
    to whatever format the file name names, so the colour type only has to be one it accepts."""
    raw = b"".join(b"\0" + b"\0" * (width * 4) for _ in range(height))
    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(png_chunk(b"IHDR", header))
        f.write(png_chunk(b"IDAT", zlib.compress(raw, 9)))
        f.write(png_chunk(b"IEND", b""))


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    up = sys.argv[1]
    with open(os.path.join(up, "assets.json"), encoding="utf-8") as f:
        assets = json.load(f)

    made = {"png": 0, "m64": 0, "bin": 0}
    skipped_sound = 0
    no_dimensions = []

    for name, data in assets.items():
        if name.startswith("@"):
            skipped_sound += 1
            continue
        meta, size = data[:-2], data[-2]
        path = os.path.join(up, name)
        kind = name.rsplit(".", 1)[-1]

        if kind == "aiff":
            skipped_sound += 1
            continue
        if kind not in made:
            sys.exit(f"gen-blank-assets: {name}: unhandled kind {kind!r}")

        os.makedirs(os.path.dirname(path), exist_ok=True)
        if kind == "png":
            if len(meta) < 2:
                no_dimensions.append(name)
                continue
            write_blank_png(path, meta[0], meta[1])
        else:
            with open(path, "wb") as f:
                f.write(b"\0" * size)
        made[kind] += 1

    for kind, n in made.items():
        print(f"  {kind:<4} {n:>5}")
    print(f"  sound entries left to upstream's own pipeline: {skipped_sound}")
    if no_dimensions:
        print(f"  no pixel dimensions, not written: {len(no_dimensions)}")
        for name in no_dimensions[:12]:
            print(f"    {name}")


if __name__ == "__main__":
    main()
