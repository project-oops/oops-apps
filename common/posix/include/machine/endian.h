/*
 * `machine/endian.h` - the BSD byte-order macros.
 *
 * FreeBSD puts these here rather than in `<endian.h>`, and a port that detects the platform by
 * `__FreeBSD__` will look here. ioquake3's `q_platform.h:213` is the first caller in this tree: it
 * includes this and then decides `Q3_LITTLE_ENDIAN` from `#if BYTE_ORDER == BIG_ENDIAN`. Without
 * it, 126 of q3rally's 132 sources stop on the include - one missing header reading as a port-wide
 * failure.
 *
 * **The values are derived from the compiler, not asserted.** clang defines `__BYTE_ORDER__` and
 * the `__ORDER_*__` constants for every target it supports, so `BYTE_ORDER` here is whatever the
 * compiler already knows. Hard-coding little-endian would be correct for x86-64 and would be a
 * lie waiting for the first big-endian target, and a byte-order macro that is wrong does not fail
 * to build - it silently swaps every integer a program reads from a file.
 *
 * The numeric values themselves (1234 / 4321 / 3412) are BSD's own and are what code comparing
 * against them expects.
 */
#ifndef OOPS_POSIX_MACHINE_ENDIAN_H
#define OOPS_POSIX_MACHINE_ENDIAN_H

#define _QUAD_HIGHWORD 1
#define _QUAD_LOWWORD  0

#ifndef LITTLE_ENDIAN
#define LITTLE_ENDIAN 1234
#endif
#ifndef BIG_ENDIAN
#define BIG_ENDIAN    4321
#endif
#ifndef PDP_ENDIAN
#define PDP_ENDIAN    3412
#endif

/* The `__` forms first: glibc-shaped code tests these, BSD-shaped code tests the plain ones, and
 * a port that includes both headers' worth of expectations needs them to agree. */
#ifndef __LITTLE_ENDIAN
#define __LITTLE_ENDIAN LITTLE_ENDIAN
#endif
#ifndef __BIG_ENDIAN
#define __BIG_ENDIAN    BIG_ENDIAN
#endif
#ifndef __PDP_ENDIAN
#define __PDP_ENDIAN    PDP_ENDIAN
#endif

#ifndef BYTE_ORDER
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__)
#  if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#    define BYTE_ORDER BIG_ENDIAN
#  elif __BYTE_ORDER__ == __ORDER_PDP_ENDIAN__
#    define BYTE_ORDER PDP_ENDIAN
#  else
#    define BYTE_ORDER LITTLE_ENDIAN
#  endif
#else
/* No compiler answer to derive from. Refuse rather than assume: a guessed byte order builds
 * cleanly and corrupts every file the program reads. */
#  error "machine/endian.h: the compiler did not define __BYTE_ORDER__, so byte order is unknown"
#endif
#endif

#ifndef __BYTE_ORDER
#define __BYTE_ORDER BYTE_ORDER
#endif

#endif /* OOPS_POSIX_MACHINE_ENDIAN_H */
