/*
 * Freestanding <inttypes.h> for the webview build's C side (QuickJS).
 *
 * The SDK's freestanding libc has <stdint.h> (via the compiler) but no <inttypes.h>, and QuickJS
 * prints fixed-width integers with the PRI* macros. On the LP64 FreeBSD target int64_t is `long`,
 * so the 64-bit conversions take the `l` length. The C side compiles with -w, so any residual
 * width mismatch is silent and harmless (long and long long are both 64-bit here).
 *
 * Part of the webview freestanding-libc workaround; filed REQ-20260925T0110Z-e1d9. Delete once the
 * SDK libc ships <inttypes.h>.
 */
#ifndef OOPSY_SHIM_INTTYPES_H
#define OOPSY_SHIM_INTTYPES_H

#include <stdint.h>

#define PRId8  "d"
#define PRIu8  "u"
#define PRIx8  "x"
#define PRId16 "d"
#define PRIu16 "u"
#define PRIx16 "x"
#define PRId32 "d"
#define PRIi32 "i"
#define PRIu32 "u"
#define PRIx32 "x"
#define PRIX32 "X"
#define PRIo32 "o"
#define PRId64 "ld"
#define PRIi64 "li"
#define PRIu64 "lu"
#define PRIx64 "lx"
#define PRIX64 "lX"
#define PRIo64 "lo"

#define SCNd32 "d"
#define SCNu32 "u"
#define SCNx32 "x"
#define SCNd64 "ld"
#define SCNu64 "lu"
#define SCNx64 "lx"

typedef struct { intmax_t quot; intmax_t rem; } imaxdiv_t;

/* QuickJS uses ssize_t but the SDK libc does not declare it. C11 permits an identical typedef
 * redefinition, so the guard is belt-and-braces. */
#ifndef _SSIZE_T_DECLARED
#define _SSIZE_T_DECLARED
typedef long ssize_t;
#endif

#endif /* OOPSY_SHIM_INTTYPES_H */
