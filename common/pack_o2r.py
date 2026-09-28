#!/usr/bin/env python3
"""Torch's `pack` command, for the port archives the libultraship titles build with it.

    pack_o2r.py <folder> <output.o2r> [major.minor.patch]

Harbour Masters' ports make their own archive (`ghostship.o2r`, `starship.o2r`) with
`torch pack port <name>.o2r o2r [-u <version>]` (`GeneratePortO2R` in each CMakeLists.txt),
which is `Companion::Pack` in `Torch/src/Companion.cpp`: every file under the folder, at its
path relative to it, into a zip (`ZWrapper`, over miniz, deflated), then - when a version is
given - a `portVersion` entry of three big-endian 16-bit numbers. `PORT_VERSION_ENDIANNESS`,
which would put a byte in front, is off by default and neither port sets it. Building the
standalone Torch for the host to do this would take its whole dependency set for a zip file.

Entries are written in sorted order with a fixed timestamp, so the same folder gives the same
archive.
"""
import os
import struct
import sys
import zipfile

FIXED_TIME = (1980, 1, 1, 0, 0, 0)


def main(folder, output, version=None):
    paths = []
    for root, _dirs, files in os.walk(folder):
        for name in files:
            full = os.path.join(root, name)
            paths.append((os.path.relpath(full, folder).replace(os.sep, "/"), full))
    paths.sort()

    tmp = output + ".tmp"
    with zipfile.ZipFile(tmp, "w", zipfile.ZIP_DEFLATED) as z:
        for rel, full in paths:
            info = zipfile.ZipInfo(rel, FIXED_TIME)
            info.compress_type = zipfile.ZIP_DEFLATED
            with open(full, "rb") as f:
                z.writestr(info, f.read())
        if version:
            major, minor, patch = (int(p) for p in version.split("."))
            info = zipfile.ZipInfo("portVersion", FIXED_TIME)
            info.compress_type = zipfile.ZIP_DEFLATED
            z.writestr(info, struct.pack(">HHH", major, minor, patch))
    os.replace(tmp, output)
    extra = f" and portVersion {version}" if version else ""
    print(f"pack_o2r: {len(paths)} files{extra} in {output}")


if __name__ == "__main__":
    main(*sys.argv[1:4])
