#!/usr/bin/env python3
"""Torch's `pack` command, for the one archive this port builds with it.

    pack_o2r.py <folder> <output.o2r> <major.minor.patch>

Upstream makes `ghostship.o2r` with `torch pack port ghostship.o2r o2r -u <version>`
(`CMakeLists.txt:990-996`), which is `Companion::Pack` in `Torch/src/Companion.cpp`: every file
under the folder, at its path relative to it, into a zip (`ZWrapper`, over miniz, deflated),
then a `portVersion` entry of three big-endian 16-bit numbers. `PORT_VERSION_ENDIANNESS`, which
would put a byte in front, is off by default and Ghostship does not set it. Building the
standalone Torch for the host to do this would take its whole dependency set for a zip file.

Entries are written in sorted order with a fixed timestamp, so the same folder gives the same
archive.
"""
import os
import struct
import sys
import zipfile

FIXED_TIME = (1980, 1, 1, 0, 0, 0)


def main(folder, output, version):
    major, minor, patch = (int(p) for p in version.split("."))
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
        info = zipfile.ZipInfo("portVersion", FIXED_TIME)
        info.compress_type = zipfile.ZIP_DEFLATED
        z.writestr(info, struct.pack(">HHH", major, minor, patch))
    os.replace(tmp, output)
    print(f"pack_o2r: {len(paths)} files and portVersion {version} in {output}")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], sys.argv[3])
