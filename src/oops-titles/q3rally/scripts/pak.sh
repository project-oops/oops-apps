#!/usr/bin/env bash
# pak.sh - pack q3rally's game data and its three bytecode modules into one .pk3.
#
#   pak.sh <basegame-dir> <qvm-build-dir> <output.pk3>
#
# Called by `make pak`; not meant to be run by hand, though it can be.
#
# # Why one archive
#
# `pros restore` costs per *file* rather than per byte - 1,775 files measured at 81 minutes - and
# `upstream/baseq3r` is 3,623 of them. As one `.pk3` it is one file, and the data can be redeployed
# in the time it takes to send the bytes.
#
# # What a .pk3 is, and the one way to get it wrong
#
# A zip, which is what ioquake3 reads natively - `qcommon/unzip.c` is minizip and is in the payload's
# source list. **The paths inside are relative to the game directory.** An archive holding
# `baseq3r/maps/foo.bsp` rather than `maps/foo.bsp` is one the engine opens successfully, finds
# nothing in, and reports as an empty pk3 - so this zips from *inside* `baseq3r`, and the check at the
# end refuses an archive whose first entry starts with `baseq3r/`.
#
# The three `.qvm` modules go in under `vm/`, which is where `VM_Create` looks. Upstream's `run.sh`
# symlinks them to the same place.
#
# # The archiver is probed by running it
#
# `common/app.mk` records why at its own zip step: Windows installs a `python3` App Execution Alias
# that exists, satisfies `command -v`, and then prints "Python was not found" and exits 49. So the
# question asked is `python3 -c 'import zipfile'`, which is what the branch actually depends on.
#
# `zip` is preferred where it exists because it is faster on a tree this size. Both deflate.
set -euo pipefail

BASEGAME="${1:?basegame dir}"
QVM_BUILD="${2:?qvm build dir}"
OUT="${3:?output pk3}"

[ -d "$BASEGAME" ] || { echo "pak: no $BASEGAME" >&2; exit 1; }

# The modules, staged so that they zip as `vm/<name>.qvm` whatever nested directory upstream's build
# put them in (`<build>/release-<platform>-<arch>/baseq3r/vm/`).
STAGE="$(dirname "$OUT")/../stage"
rm -rf "$STAGE"
mkdir -p "$STAGE/vm" "$(dirname "$OUT")"

qvms=$(find "$QVM_BUILD" -name '*.qvm' | wc -l)
[ "$qvms" -eq 3 ] || { echo "pak: expected 3 qvm modules under $QVM_BUILD, found $qvms" >&2; exit 1; }
find "$QVM_BUILD" -name '*.qvm' -exec cp {} "$STAGE/vm/" \;

rm -f "$OUT"
echo "pak: packing $(du -sh "$BASEGAME" | cut -f1) of baseq3r plus 3 modules"

if command -v zip >/dev/null 2>&1; then
    ( cd "$BASEGAME" && zip -qr "$OUT" . -x '.*' )
    ( cd "$STAGE" && zip -qr "$OUT" vm )
elif python3 -c 'import zipfile' >/dev/null 2>&1; then
    python3 - "$OUT" "$BASEGAME" "$STAGE" <<'PY'
import os, sys, zipfile

out, basegame, stage = sys.argv[1], sys.argv[2], sys.argv[3]

def add(zf, root, skip_dotfiles):
    n = 0
    for dirpath, dirnames, filenames in os.walk(root):
        if skip_dotfiles:
            dirnames[:] = [d for d in dirnames if not d.startswith(".")]
        for name in filenames:
            if skip_dotfiles and name.startswith("."):
                continue
            full = os.path.join(dirpath, name)
            # Relative to `root`, which is what makes the entry `maps/foo.bsp` rather than
            # `baseq3r/maps/foo.bsp` - see the note at the top about which of those works.
            zf.write(full, os.path.relpath(full, root).replace(os.sep, "/"))
            n += 1
    return n

with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as zf:
    a = add(zf, basegame, True)
    b = add(zf, stage, False)
print("pak: %d data entries, %d modules" % (a, b))
PY
else
    echo "pak: cannot pack - install 'zip', or a python3 that runs" >&2
    exit 1
fi

rm -rf "$STAGE"

# **The check that the paths are right**, because the failure it catches is silent: the engine opens
# a wrongly-rooted pk3 without complaint and simply has no files in it.
if python3 -c 'import zipfile' >/dev/null 2>&1; then
    python3 - "$OUT" <<'PY'
import sys, zipfile
names = zipfile.ZipFile(sys.argv[1]).namelist()
bad = [n for n in names if n.startswith("baseq3r/")]
if bad:
    sys.exit("pak: %d entries are rooted at baseq3r/ - the engine would read this as empty (%s)"
             % (len(bad), bad[0]))
for need in ("default.cfg", "vm/qagame.qvm", "vm/cgame.qvm", "vm/ui.qvm"):
    if need not in names:
        sys.exit("pak: %s is missing from the archive" % need)
print("pak: %d entries, paths rooted correctly, all three modules present" % len(names))
PY
else
    echo "pak: no python3 to verify the archive's paths - check them by hand" >&2
fi

echo "pak: $OUT ($(du -h "$OUT" | cut -f1))"
