#!/usr/bin/env bash
# Build the Neverball tree's map compiler on the host, and compile its levels - Neverputt's
# courses among them.
#
# **A verbatim copy of `../../neverball/scripts/mapc.sh`.** The two titles hold their own
# checkouts of the same upstream (see `upstream.lock` for why), so each needs its own run against
# its own `data/`, and the script derives every path from its own location - `HERE`, `TITLE`,
# `DEPS` - so the copy needs no edit beyond this note. Shared machinery belongs in `common/`, but
# this is specific to one upstream project rather than to every app here, and a title reaching
# into a sibling title's `scripts/` is the coupling `upstream.lock` explains avoiding. If a third
# title ever comes out of this tree, that is the moment to lift it somewhere shared.
#
# It compiles **every** `.map` in the tree, Neverball's levels included: splitting it by binary
# would mean teaching it which courses belong to which title, and the tool costs minutes either
# way. `make package` is what selects.
#
# Neverball ships `.map` sources and no `.sol`: the levels are compiled by `mapc`, a tool that
# runs on the **build machine**, not on the console. Without this the payload has no levels at
# all, so this is part of building the title rather than an optional extra.
#
# # Why it builds its own zlib, libpng and libjpeg
#
# `mapc` reads PNG and JPEG textures to work out their sizes, so it needs those libraries -
# natively, for the build machine. The builder has no dev packages for them, and installing some
# would put a dependency outside the lock files that everything else here obeys.
#
# So it compiles the **same pinned sources** the payload uses, with the host compiler and no
# target flags. Same revisions, same code, different machine - which also means a bump to those
# locks is picked up here without anybody remembering to.
#
# # The JPEG wrappers
#
# libjpeg-turbo 3.x compiles a dozen of its sources once per sample precision, through one-line
# wrappers that CMake generates from `src/wrapper/template.c`. We do not run its CMake, so they
# are written below - two lines each, exactly what the template produces. Without them the link
# fails on eighteen undefined `j12*` symbols, because `jdmaster.c` dispatches on an image's
# precision at run time and so needs all of them to exist.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TITLE="$(dirname "$HERE")"
DEPS="$(cd "$TITLE/../../oops-deps" && pwd)"
NB="$TITLE/upstream"
OUT="$TITLE/build/mapc"

[ -d "$NB/data" ] || { echo "mapc: no upstream tree - run make first to fetch it" >&2; exit 1; }

Z="$DEPS/zlib/upstream"
P="$DEPS/libpng/upstream"
J="$DEPS/libjpeg-turbo/upstream"
INC="-I$Z -I$P -I$DEPS/libpng/include -I$J/src -I$DEPS/libjpeg-turbo/include"
CF="-O2 -w -DZ_SOLO"

mkdir -p "$OUT/wrapper"

if [ ! -x "$OUT/mapc" ]; then
    echo "mapc: building the host tool"

    for f in jdapistd jdcolor jddiffct jdlossls jdmainct jdpostct jdsample jutils; do
        for b in 8 12 16; do
            printf '#define BITS_IN_JSAMPLE %s\n#include "%s/src/%s.c"\n' "$b" "$J" "$f" \
                > "$OUT/wrapper/$f-$b.c"
        done
    done
    for f in jdcoefct jddctmgr jdmerge jidctflt jidctfst jidctint jidctred jquant1 jquant2; do
        for b in 8 12; do
            printf '#define BITS_IN_JSAMPLE %s\n#include "%s/src/%s.c"\n' "$b" "$J" "$f" \
                > "$OUT/wrapper/$f-$b.c"
        done
    done

    n=0
    for f in "$Z"/adler32.c "$Z"/crc32.c "$Z"/deflate.c "$Z"/inflate.c "$Z"/inftrees.c \
             "$Z"/inffast.c "$Z"/trees.c "$Z"/zutil.c "$Z"/compress.c "$Z"/uncompr.c \
             "$P"/png.c "$P"/pngerror.c "$P"/pngget.c "$P"/pngmem.c "$P"/pngpread.c \
             "$P"/pngread.c "$P"/pngrio.c "$P"/pngrtran.c "$P"/pngrutil.c "$P"/pngset.c \
             "$P"/pngtrans.c "$P"/pngwio.c "$P"/pngwrite.c "$P"/pngwtran.c "$P"/pngwutil.c; do
        n=$((n + 1)); cc $CF $INC -c -o "$OUT/z$n.o" "$f"
    done
    for s in jcomapi jdapimin jdatasrc jdhuff jdinput jdmarker jdmaster jdtrans jerror \
             jmemmgr jmemnobs jdphuff jdicc jdlhuff; do
        n=$((n + 1)); cc $CF $INC -c -o "$OUT/z$n.o" "$J/src/$s.c"
    done
    for f in "$OUT"/wrapper/*.c; do
        n=$((n + 1)); cc $CF $INC -c -o "$OUT/z$n.o" "$f"
    done

    # mapc's own sources, which upstream's Makefile lists as MAPC_OBJS - plus `fs_stdio.c`, which
    # the `ENABLE_FS=stdio` branch of that Makefile adds.
    m=0
    for s in vec3 base_image solid_base binary base_config common fs_common fs_stdio fs_png \
             fs_jpg dir array list mapc; do
        m=$((m + 1))
        cc $CF $INC -I"$NB/share" -I"$DEPS/sdl2/upstream/include" -c -o "$OUT/m$m.o" \
            "$NB/share/$s.c"
    done

    cc -o "$OUT/mapc" "$OUT"/m*.o "$OUT"/z*.o -lm
    echo "mapc: built from $n library and $m tool objects"
fi

# `mapc <file>.map data` writes the `.sol` into the data tree, which is where the game looks.
cd "$NB"
total=0; built=0
for map in $(find data -name '*.map' | sort); do
    total=$((total + 1))
    if "$OUT/mapc" "$map" data >/dev/null 2>&1; then built=$((built + 1)); else
        echo "mapc: FAILED $map" >&2
    fi
done
echo "mapc: $built of $total levels compiled, $(find data -name '*.sol' | wc -l) .sol on disk"
