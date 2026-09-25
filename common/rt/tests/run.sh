#!/bin/sh
# Build and run the `common/rt` test on the host.
#
# **The renames are the point of this script.** `rt.c` implements the helpers that the host's own
# `/` and `%` on a 128-bit value compile down to, so linking it in under its real names makes the
# test compare a function against itself. Compiling it as `oops_test_*` leaves the operators here
# bound to the toolchain's compiler-rt, which is an implementation nobody in this tree wrote.
#
# `rt_test.c` re-checks this at run time and refuses to report a pass without it.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
CC=${CC:-clang}
OUT=${OUT:-/tmp/oops-rt-test}

"$CC" -O2 -Wall -Wextra -Werror -c -o "$OUT-rt.o" \
    -D__udivti3=oops_test_udivti3 \
    -D__umodti3=oops_test_umodti3 \
    -D__udivmodti4=oops_test_udivmodti4 \
    "$HERE/../rt.c"
"$CC" -O2 -Wall -Wextra -Werror -o "$OUT" "$HERE/rt_test.c" "$OUT-rt.o"
"$OUT"
