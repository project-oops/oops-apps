#!/usr/bin/env bash
#
# Build and package several demos in one go, keeping each packaged title tree.
#
#   bash <title>/tools/stage-set.sh cubemap fbotexture shadowtex stex3d
#
# `make title` always writes `build/title/$(TITLE_ID)`, one id for every demo, so each packaged
# tree is kept under `build/staged/<demo>`. It does not deploy; `pros restore <staged dir>
# /data/homebrew/<id>` does.
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TITLE="$(dirname "$HERE")"
cd "$TITLE" || exit 1

[ $# -gt 0 ] || { echo "usage: stage-set.sh <demo> [demo...]"; exit 2; }

ID="$(grep -E '^TITLE_ID=' app.env | cut -d= -f2)"
[ -n "$ID" ] || { echo "stage-set: no TITLE_ID in app.env"; exit 1; }

mkdir -p build/staged
fail=0

for d in "$@"; do
    printf '=== %s\n' "$d"

    rm -rf "build/title/$ID"
    rm -f build/mesa-demos.elf build/imports.txt

    if ! make DEMO="$d" > "build/staged/$d.log" 2>&1; then
        echo "    BUILD FAILED - see build/staged/$d.log"
        fail=1
        continue
    fi
    if ! make DEMO="$d" imports >> "build/staged/$d.log" 2>&1; then
        echo "    IMPORTS FAILED - see build/staged/$d.log"
        fail=1
        continue
    fi
    if ! make DEMO="$d" title >> "build/staged/$d.log" 2>&1; then
        echo "    PACKAGE FAILED - see build/staged/$d.log"
        fail=1
        continue
    fi

    rm -rf "build/staged/$d"
    cp -r "build/title/$ID" "build/staged/$d"
    printf '    staged  build/staged/%s  (%s)\n' "$d" "$(du -sh "build/staged/$d" | cut -f1)"
done

exit "$fail"
