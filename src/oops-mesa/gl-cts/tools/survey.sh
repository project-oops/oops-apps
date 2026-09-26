#!/bin/sh
# Compile every CTS framework and test-module source for the target, one at a time, and group
# the failures by cause (as for the libc++ survey, oops-apps#D007). Nothing is linked or
# installed.
#
#   tools/survey.sh            all of it
#   tools/survey.sh tcuDefs    just the sources whose name contains that
set -u

HERE=$(cd "$(dirname "$0")/.." && pwd)
OOPS_APPS=$(cd "$HERE/../../.." && pwd)
SDK=$(cd "$OOPS_APPS/../oops-sdk" && pwd)
LIBCXX="$OOPS_APPS/src/oops-deps/libcxx"
UP="$HERE/upstream/framework"
MOD="$HERE/upstream/external/openglcts/modules"
GLS="$HERE/upstream/modules/glshared"
OUT="${TMPDIR:-/tmp}/gl-cts-survey"

rm -rf "$OUT"; mkdir -p "$OUT"

INCLUDES="-I$UP/delibs/debase -I$UP/delibs/depool -I$UP/delibs/deutil
          -I$UP/delibs/dethread -I$UP/delibs/decpp -I$UP/delibs/deimage
          -I$UP/qphelper -I$UP/common -I$UP/opengl -I$UP/referencerenderer
          -I$UP/egl -I$HERE/shim/include
          -I$UP/opengl/wrapper -I$UP/egl/wrapper
          -I$UP/opengl/simplereference -I$UP/randomshaders
          -I$UP/xexml"

# Every test-module directory holding a header, the same set `Makefile`'s `CTS_INCLUDES`
# builds: upstream's includes are flat, so the set must be complete.
for d in $(find "$MOD" "$GLS" -name '*.hpp' | sed 's|/[^/]*$||' | sort -u); do
    INCLUDES="$INCLUDES -I$d"
done

# A hosted title takes its C library from the Mesa sysroot, whose types collide with oops-sdk's
# freestanding libc (`common/app.mk`, USE_MESA). oops-sdk's non-libc headers (`oops/gfx.h`)
# stay on the path.
MESA_SYSROOT="$OOPS_APPS/../oops-mesa/toolchain/sysroot"

# `<fenv.h>` from msun's source tree (`msun/x86/fenv.h` on amd64), because the sysroot has
# none. `deMath.c` sets the rounding mode to compute GLSL rounding reference values.
MESA_MSUN_X86="$OOPS_APPS/../oops-mesa/toolchain/msun-src/msun/x86"

# `_XOPEN_SOURCE=600`, as in `Makefile`: at least 500 for `deThreadUnix.c:32`, at most 600
# because FreeBSD's `<unistd.h>` hides `usleep` above it, and `__BSD_VISIBLE` 0 keeps its
# `u_long` declarations out of units without `<sys/types.h>`.
BASE="-target x86_64-unknown-freebsd --sysroot=$MESA_SYSROOT
      -D_XOPEN_SOURCE=600
      -fPIC -fno-stack-protector -O2 -w -DOOPS_TARGET=3
      -I$SDK/include -I$MESA_MSUN_X86
      $INCLUDES"

# libc++'s headers come first: its `<math.h>` and friends wrap the C headers with
# `#include_next`, and `<cmath>` refuses to build without them. `-std=` must match
# `Makefile`'s `OOPS_CXX_STD`; change both together.
CXXFLAGS="-nostdinc++ -fexceptions -frtti -std=c++17
          -I$LIBCXX/include -I$LIBCXX/upstream/libcxx/include
          -DDEQP_TARGET_NAME=\"OOPS\" $BASE"
# `deMemory.c` calls `malloc_usable_size`, which FreeBSD declares in `<malloc_np.h>`, not
# `<stdlib.h>`; force-including it leaves `<stdlib.h>` unshadowed.
CFLAGS="-std=c11 -DDEQP_TARGET_NAME=\"OOPS\" -include malloc_np.h $BASE"

filter="${1:-}"
ok=0; bad=0

# Skipped, and counted: `framework/platform` (other operating systems; ours is `shim/`) and
# what `Makefile`'s `CTS_MOD_EXCLUDE` leaves out, so the histogram lists only open work.
skipped=0
for src in $(find "$UP" "$MOD" "$GLS" \( -name '*.cpp' -o -name '*.c' \) | sort); do
    name=$(basename "$src")
    case "$name" in *pch*) continue;; esac
    case "$src" in "$UP"/platform/*) skipped=$((skipped+1)); continue;; esac
    case "$src" in
        "$MOD"/runner/*|"$MOD"/glcTestPackageRegistry.cpp|"$MOD"/common/glcSpirvUtils.cpp \
        |"$MOD"/gl/gl4cContextFlushControlTests.cpp)
            skipped=$((skipped+1)); continue;;
    esac
    if [ -n "$filter" ]; then
        case "$src" in *"$filter"*) ;; *) continue;; esac
    fi

    stem=$(echo "${src#$HERE/upstream/}" | tr '/' '_')
    case "$name" in
        *.cpp) cc="clang++ $CXXFLAGS";;
        *.c)   cc="clang $CFLAGS";;
    esac

    if $cc -c -o "$OUT/$stem.o" "$src" 2>"$OUT/$stem.err"; then
        ok=$((ok+1))
    else
        bad=$((bad+1))
        why=$(grep -m1 -oE "'[^']+' file not found" "$OUT/$stem.err")
        [ -z "$why" ] && why=$(grep -m1 -E "error:" "$OUT/$stem.err" \
                               | sed 's/.*error: //' | cut -c1-64)
        [ -z "$why" ] && why="(no error line; see $OUT/$stem.err)"
        printf '%s\t%s\n' "$why" "${src#$HERE/upstream/}" >> "$OUT/failures.tsv"
    fi
done

echo
if [ -f "$OUT/failures.tsv" ]; then
    echo "causes, most files first:"
    cut -f1 "$OUT/failures.tsv" | sort | uniq -c | sort -rn | head -20
    echo
fi
echo "compiles: $ok   fails: $bad   of $((ok+bad))   (skipped $skipped in platform/)"
echo "per-file errors in $OUT"
