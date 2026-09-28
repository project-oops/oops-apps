"""Refuse a `soh.o2r` that was built from a different upstream than the one pinned.

`soh.o2r` is the port's own assets and is tracked in the repository, because producing it
needs a host build of ZAPD that CI does not have - so it is the one artifact here that is
not rebuilt from the pinned source on every build. That is exactly the shape of thing that
goes quietly wrong: bump `upstream.lock`, and the archive is still the old one, still loads,
and disagrees with the code around it in ways that surface as missing assets rather than as
an error.

Upstream writes a `portVersion` entry into the archive for its own version checks -
`OTRGlobals` reads it to decide whether an archive needs regenerating. Its layout is in
`OTRExporter/Main.cpp`: one byte saying which endianness the rest is in, then major, minor
and patch as three 16-bit numbers in that endianness. Seven bytes, so the version can be
compared against the lock's `UPSTREAM_REF` without unpacking anything else.

Usage: check-o2r-version.py <soh.o2r> <expected version, e.g. 9.2.3>
"""

import struct
import sys
import zipfile


def archive_version(path):
    """The (major, minor, patch) `portVersion` records, or None if it says nothing."""
    with zipfile.ZipFile(path) as z:
        try:
            raw = z.read("portVersion")
        except KeyError:
            return None

    # One byte of endianness, then three 16-bit numbers in it. Anything shorter is not a
    # version. The marker is read rather than assumed: upstream writes big-endian today
    # (`Endianness::Big` is 1), and the byte is there precisely so a reader need not care.
    if len(raw) < 7:
        return None
    order = ">" if raw[0] == 1 else "<"
    major, minor, patch = struct.unpack_from(order + "HHH", raw, 1)
    return (major, minor, patch)


def main():
    if len(sys.argv) != 3:
        sys.stderr.write("usage: check-o2r-version.py <soh.o2r> <expected>\n")
        return 2

    path, expected = sys.argv[1], sys.argv[2]

    try:
        parts = tuple(int(p) for p in expected.split("."))
    except ValueError:
        sys.stderr.write(
            "check-o2r-version: '{}' is not a version like 9.2.3\n".format(expected)
        )
        return 1
    if len(parts) != 3:
        sys.stderr.write(
            "check-o2r-version: '{}' is not a version like 9.2.3\n".format(expected)
        )
        return 1

    got = archive_version(path)
    if got is None:
        sys.stderr.write(
            "check-o2r-version: {} carries no readable portVersion, so it cannot be\n"
            "  matched against the pinned upstream. Regenerate it - docs/PORTING.md.\n".format(
                path
            )
        )
        return 1

    if got != parts:
        sys.stderr.write(
            "check-o2r-version: {} was built from upstream {}, and this tree is pinned to\n"
            "  {}. The archive is tracked rather than rebuilt, so a lock bump leaves it\n"
            "  behind. Regenerate it - docs/PORTING.md - and commit the new one.\n".format(
                path, ".".join(str(p) for p in got), expected
            )
        )
        return 1

    sys.stdout.write(
        "ship-of-harkinian: soh.o2r matches upstream {}\n".format(expected)
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
