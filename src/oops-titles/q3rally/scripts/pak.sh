#!/usr/bin/env bash
# pak.sh - pack q3rally's game data and its three bytecode modules into one .pk3.
#
#   pak.sh <basegame-dir> <qvm-build-dir> <output.pk3>
#
# Called by `make pak`. `pros restore` costs per file, so the data deploys as one archive.
#
# A `.pk3` is a zip (read by `qcommon/unzip.c`). Paths inside are relative to the game
# directory: an entry rooted at `baseq3r/` is never found, so this zips from inside
# `baseq3r` and the final check refuses such entries. The `.qvm` modules go under `vm/`,
# where `VM_Create` looks.
#
# python3 is probed by running it, because a Windows App Execution Alias satisfies
# `command -v` and then fails (see `common/app.mk`). `zip` is preferred for speed.
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
            # Relative to `root`: `maps/foo.bsp`, not `baseq3r/maps/foo.bsp`.
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

# Verify the paths: the engine opens a wrongly rooted pk3 without complaint and finds nothing.
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
