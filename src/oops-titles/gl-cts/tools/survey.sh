#!/bin/sh
# Compile every CTS framework source for the target, one at a time, and group the failures by
# cause.
#
# This is the same instrument as the libc++ survey behind `oops-apps#D007`, for the same reason:
# a port of 260-odd files does not need its first error, it needs to know which twelve causes
# account for all of them. Nothing is linked and nothing is installed.
#
#   tools/survey.sh            all of it
#   tools/survey.sh tcuDefs    just the sources whose name contains that
set -u

HERE=$(cd "$(dirname "$0")/.." && pwd)
OOPS_APPS=$(cd "$HERE/../../.." && pwd)
SDK=$(cd "$OOPS_APPS/../oops-sdk" && pwd)
LIBCXX="$OOPS_APPS/src/oops-deps/libcxx"
UP="$HERE/upstream/framework"
OUT="${TMPDIR:-/tmp}/gl-cts-survey"

rm -rf "$OUT"; mkdir -p "$OUT"

INCLUDES="-I$UP/delibs/debase -I$UP/delibs/depool -I$UP/delibs/deutil
          -I$UP/delibs/dethread -I$UP/delibs/decpp -I$UP/delibs/deimage
          -I$UP/qphelper -I$UP/common -I$UP/opengl -I$UP/referencerenderer
          -I$UP/egl -I$HERE/shim/include
          -I$UP/opengl/wrapper -I$UP/egl/wrapper
          -I$UP/xexml"

# # This is a HOSTED title, so the C library is the Mesa sysroot's
#
# `common/app.mk` puts it plainly at its `USE_MESA` block: a hosted title takes its target C
# library from the Mesa sysroot, and oops-sdk's freestanding libc headers **collide** with it -
# the sysroot's `__clock_t` is `int` where oops-sdk's `clock_t` is `int64_t`. So `app.mk` empties
# `OOPS_SDK_LIBC_INCLUDE` for these titles.
#
# This survey used oops-sdk's freestanding libc until 2026-09-23 and was measuring a
# configuration this title will never be built in. The sysroot is a full FreeBSD header set -
# `unistd.h`, `signal.h`, `pthread.h`, `dirent.h`, `sys/stat.h` and a real `libm.a` - so most of
# what looked like a long list of SDK gaps was the wrong question rather than missing work.
#
# oops-sdk's *non-libc* headers stay: `oops/gfx.h` and friends are how the platform layer reaches
# the display, and they are not a C library.
MESA_SYSROOT="$OOPS_APPS/../oops-mesa/toolchain/sysroot"

# # `<fenv.h>`, which the staged sysroot does not carry and msun's source tree does
#
# `deMath.c` sets the floating-point rounding mode through `fegetround`/`fesetround` to compute
# reference values for the GLSL rounding tests, so it is not optional and a wrong one would make
# a wrong reference rather than a failure.
#
# The sysroot has no `fenv.h` - only libc++'s wrapper, which `#include_next`s a C one that is not
# there. FreeBSD keeps it per-architecture and amd64 uses `msun/x86/fenv.h`, which oops-mesa
# already stages from the same pinned checkout as `libm.a`. So this points at the real header
# rather than writing a second one: the rounding-mode constants are the x87 control-word values
# and the SSE shift is 3, and neither is worth transcribing by hand.
#
# **This reaches past the sysroot into the source tree beside it, and should not have to.**
# Staging `fenv.h` into `sysroot/usr/include` is oops-mesa's to do; when it does, this line goes.
MESA_MSUN_X86="$OOPS_APPS/../oops-mesa/toolchain/msun-src/msun/x86"

# # `_XOPEN_SOURCE=600`, and the value is pinned from both sides
#
# It has to be **at least 500**, because `deThreadUnix.c:32` is
# `#if !defined(_XOPEN_SOURCE) || (_XOPEN_SOURCE < 500)` over `#error "You are using too old
# posix API!"`. (`_POSIX_C_SOURCE` is a different macro and does not satisfy that check - an
# easy hour to lose.)
#
# It has to be **at most 600**, because FreeBSD's `<unistd.h>` guards `usleep` with
# `(__XSI_VISIBLE && __XSI_VISIBLE <= 600) || __BSD_VISIBLE`: POSIX 2008 removed it, so 700
# hides it and `deThreadUnix.c` calls it.
#
# Setting it also drives `__BSD_VISIBLE` to 0, which is separately necessary: FreeBSD's
# `<unistd.h>` declares `fflagstostr(u_long)` and `select(..., fd_set *, ...)` in BSD-visible
# blocks *without including `<sys/types.h>` itself*, so a unit including `<unistd.h>` alone meets
# them with no `u_long` in scope. `oops-libcxx.mk` reaches the same place through
# `_POSIX_C_SOURCE`, which is right for it because libc++ never calls `usleep`.
BASE="-target x86_64-unknown-freebsd --sysroot=$MESA_SYSROOT
      -D_XOPEN_SOURCE=600
      -fPIC -fno-stack-protector -O2 -w -DOOPS_TARGET=3
      -I$SDK/include -I$MESA_MSUN_X86
      $INCLUDES"

# # libc++'s headers come before the SDK's, and the order is not cosmetic
#
# libc++ ships its own `<math.h>`, `<string.h>`, `<errno.h>` and friends: thin wrappers that
# pull in the C library's with `#include_next` and then add the C++ overloads. `<cmath>` checks
# that its wrapper was the one found, and says so when it was not:
#
#     <cmath> tried including <math.h> but didn't find libc++'s <math.h>
#
# With `-I$SDK/include/libc` first, the C header wins and **161 of the framework's 260 sources
# failed on that one line**. It is also why `common/cxxrt.cpp` declares `std::set_terminate`
# by hand instead of including `<exception>`, which was read at the time as a quirk of that file
# rather than as this.
CXXFLAGS="-nostdinc++ -fexceptions -frtti -std=c++17
          -I$LIBCXX/include -I$LIBCXX/upstream/libcxx/include
          -DDEQP_TARGET_NAME=\"OOPS\" $BASE"
# `-include malloc_np.h`: `deMemory.c` calls `malloc_usable_size`, which FreeBSD declares in
# `<malloc_np.h>` - the jemalloc extensions - and not in `<stdlib.h>`, which is what upstream
# includes on this platform. The sysroot has the header; this puts its declaration in scope
# without shadowing `<stdlib.h>`, which is the trap this port has already fallen into once.
CFLAGS="-std=c11 -DDEQP_TARGET_NAME=\"OOPS\" -include malloc_np.h $BASE"

filter="${1:-}"
ok=0; bad=0

# `framework/platform` holds one subdirectory per operating system - X11, Win32, Android, OSX -
# and none of them is this one. Ours is what has to be written, and it goes in `shim/`, so
# compiling upstream's here would only report that we are not Linux. Skipped rather than
# reported, with the count printed at the end so it is visible rather than silent.
skipped=0
for src in $(find "$UP" \( -name '*.cpp' -o -name '*.c' \) | sort); do
    name=$(basename "$src")
    case "$name" in *pch*) continue;; esac
    case "$src" in "$UP"/platform/*) skipped=$((skipped+1)); continue;; esac
    if [ -n "$filter" ]; then
        case "$src" in *"$filter"*) ;; *) continue;; esac
    fi

    stem=$(echo "${src#$UP/}" | tr '/' '_')
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
        printf '%s\t%s\n' "$why" "${src#$UP/}" >> "$OUT/failures.tsv"
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
