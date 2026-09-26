/* <inttypes.h> for a freestanding target, because oops-sdk's libc does not carry one
 * and libunwind includes it.
 *
 * libunwind uses this header for the `PRI*` macros only, in its logging and trace paths
 * (`_LIBUNWIND_LOG`, `_LIBUNWIND_TRACE_UNWINDING`). It needs no conversion functions,
 * so this declares none: `strtoimax` and friends are absent rather than stubbed, which
 * is the honest shape and fails at the link rather than at run time if anything ever
 * reaches for them.
 *
 * This lives here rather than in oops-sdk for the same reason `__config_site` does - it
 * is a header this dependency needs to build, not a gap in the SDK's own libc. Adding
 * it there is a different repository's call and would put a header in the SDK that
 * nothing in the SDK uses.
 *
 * The widths are LP64, which is what `-target x86_64-unknown-freebsd` is: `long` is
 * 64-bit, so the 64-bit and pointer-sized macros take the `l` modifier and the rest
 * take none.
 */
#ifndef OOPS_LIBCXX_INTTYPES_H
#define OOPS_LIBCXX_INTTYPES_H

#include <stdint.h>

#define __OOPS_PRI64 "l"

#define PRId8 "d"
#define PRIi8 "i"
#define PRIo8 "o"
#define PRIu8 "u"
#define PRIx8 "x"
#define PRIX8 "X"

#define PRId16 "d"
#define PRIi16 "i"
#define PRIo16 "o"
#define PRIu16 "u"
#define PRIx16 "x"
#define PRIX16 "X"

#define PRId32 "d"
#define PRIi32 "i"
#define PRIo32 "o"
#define PRIu32 "u"
#define PRIx32 "x"
#define PRIX32 "X"

#define PRId64 __OOPS_PRI64 "d"
#define PRIi64 __OOPS_PRI64 "i"
#define PRIo64 __OOPS_PRI64 "o"
#define PRIu64 __OOPS_PRI64 "u"
#define PRIx64 __OOPS_PRI64 "x"
#define PRIX64 __OOPS_PRI64 "X"

#define PRIdPTR __OOPS_PRI64 "d"
#define PRIiPTR __OOPS_PRI64 "i"
#define PRIoPTR __OOPS_PRI64 "o"
#define PRIuPTR __OOPS_PRI64 "u"
#define PRIxPTR __OOPS_PRI64 "x"
#define PRIXPTR __OOPS_PRI64 "X"

#define PRIdMAX __OOPS_PRI64 "d"
#define PRIiMAX __OOPS_PRI64 "i"
#define PRIoMAX __OOPS_PRI64 "o"
#define PRIuMAX __OOPS_PRI64 "u"
#define PRIxMAX __OOPS_PRI64 "x"
#define PRIXMAX __OOPS_PRI64 "X"

#endif /* OOPS_LIBCXX_INTTYPES_H */
