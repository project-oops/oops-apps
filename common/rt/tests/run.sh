#!/bin/sh
# Build and run the common/rt test on the host.
#
# rt.c is compiled under oops_test_* names so the host's own 128-bit / and % stay bound
# to the toolchain's compiler-rt; under the real names the test would compare rt.c
# against itself. rt_test.c re-checks this at run time.
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
