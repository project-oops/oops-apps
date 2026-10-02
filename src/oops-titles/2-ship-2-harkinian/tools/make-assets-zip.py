"""Pack the asset definitions ZAPD reads while it converts a ROM.

ZAPD does not read a cartridge and work out what is in it: it reads XML saying what lives
at which offset, one set per ROM version. Shipping them as a file tree is wrong twice over -
`pros restore` costs per file rather than per byte, so a tree takes hours to put on a console,
and the console has no use for thousands of files it reads once.

So they ship as one archive, which the title unpacks on its first conversion. XML deflates
about ten to one, so the package grows by a few megabytes rather than fifty.

The layout inside the archive is the one `Extractor::CallZapd` asks for, which is not the
layout upstream stores: it reads `assets/xml/<version>`, `assets/Config_<version>.xml`,
`assets/filelists`, `assets/symbols` and `assets/EnumData.xml`, while those live under
`assets/extractor/` in the tree. They are flattened here rather than on the device, so the
device just unpacks.

Which versions have to be in it is not a judgement call either - it is the set of strings
`Extractor::GetZapdVerStr` can return, because ZAPD is handed one of those and reads the
directory named after it. Reading them out of the source rather than restating them here
is what makes this a check: a version added upstream that this does not ship fails the
build, instead of failing on a console in front of somebody holding that cartridge.

Usage: make-assets-zip.py <upstream/mm/assets> <Extract.cpp> <out.zip>
"""

import os
import re
import sys
import zipfile


def zapd_versions(extract_cpp):
    """The version names `GetZapdVerStr` can return, in source order."""
    with open(extract_cpp, "r", encoding="utf-8", errors="replace") as f:
        text = f.read()

    start = text.find("GetZapdVerStr")
    if start < 0:
        raise SystemExit("make-assets-zip: no GetZapdVerStr in {}".format(extract_cpp))
    # The function ends at the first closing brace in the first column after it.
    end = text.find("\n}", start)
    body = text[start : end if end > 0 else len(text)]

    names = re.findall(r'return\s+"([A-Za-z0-9_]+)"\s*;', body)
    if not names:
        raise SystemExit("make-assets-zip: GetZapdVerStr returned no version names")
    return names


def add_tree(zf, src_dir, dest_prefix):
    """Add every file under src_dir to the archive at dest_prefix, keeping structure."""
    count = 0
    for root, _dirs, files in os.walk(src_dir):
        for name in sorted(files):
            full = os.path.join(root, name)
            rel = os.path.relpath(full, src_dir).replace(os.sep, "/")
            zf.write(full, "{}/{}".format(dest_prefix, rel))
            count += 1
    return count


def main():
    if len(sys.argv) != 4:
        sys.stderr.write("usage: make-assets-zip.py <assets-dir> <Extract.cpp> <out.zip>\n")
        return 2

    assets, extract_cpp, out = sys.argv[1], sys.argv[2], sys.argv[3]
    xml_dir = os.path.join(assets, "xml")
    extractor_dir = os.path.join(assets, "extractor")

    for needed in (xml_dir, extractor_dir):
        if not os.path.isdir(needed):
            sys.stderr.write("make-assets-zip: {} is not there\n".format(needed))
            return 1

    # Every version ZAPD can be asked for needs both halves, and it asks for whichever one
    # the player's cartridge turns out to be.
    missing = []
    for version in zapd_versions(extract_cpp):
        if not os.path.isdir(os.path.join(xml_dir, version)):
            missing.append("xml/{}/".format(version))
        if not os.path.isfile(os.path.join(extractor_dir, "Config_{}.xml".format(version))):
            missing.append("Config_{}.xml".format(version))
    if missing:
        sys.stderr.write(
            "make-assets-zip: ZAPD can ask for these and they are not in the tree:\n"
        )
        for name in missing:
            sys.stderr.write("  {}\n".format(name))
        return 1

    total = 0
    tmp = out + ".tmp"
    # Deflate, not store: the whole point is that this does not ship as 50MB.
    with zipfile.ZipFile(tmp, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
        total += add_tree(zf, xml_dir, "assets/xml")

        # `filelists`, `symbols` and the per-version `Config_*.xml` sit under `extractor/` in the
        # tree and directly under `assets/` in what ZAPD reads.
        filelists = os.path.join(extractor_dir, "filelists")
        if not os.path.isdir(filelists):
            sys.stderr.write("make-assets-zip: no {}\n".format(filelists))
            return 1
        total += add_tree(zf, filelists, "assets/filelists")

        symbols = os.path.join(extractor_dir, "symbols")
        if os.path.isdir(symbols):
            total += add_tree(zf, symbols, "assets/symbols")

        for name in sorted(os.listdir(extractor_dir)):
            if name.endswith(".xml"):
                zf.write(os.path.join(extractor_dir, name), "assets/" + name)
                total += 1

    os.replace(tmp, out)
    size_mb = os.path.getsize(out) / (1024.0 * 1024.0)
    sys.stdout.write(
        "2-ship-2-harkinian: {} asset files -> {} ({:.1f} MB)\n".format(
            total, os.path.basename(out), size_mb
        )
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
