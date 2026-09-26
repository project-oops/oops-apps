/*
 * libjpeg-turbo's public build configuration, in place of the one CMake generates from
 * `src/jconfig.h.in`.
 *
 * `JPEG_LIB_VERSION 62` is the ABI a program written against the original IJG libjpeg
 * expects, as Neverball is.
 *
 * Arithmetic coding is off: almost no JPEG uses it. SIMD is off: it is per-architecture
 * NASM assembly, and the C path decodes the same images.
 */
#ifndef OOPS_JCONFIG_H
#define OOPS_JCONFIG_H

#define JPEG_LIB_VERSION 62
#define LIBJPEG_TURBO_VERSION 3.1.4
#define LIBJPEG_TURBO_VERSION_NUMBER 3001004

#define MEM_SRCDST_SUPPORTED 1

/*
 * Guarded as in upstream's `jconfig.h.in`: the per-precision wrappers define
 * `BITS_IN_JSAMPLE` as 12 or 16 before including the real source, and an unguarded
 * define would compile every pass as 8-bit and leave the `j12*` symbols undefined.
 */
#ifndef BITS_IN_JSAMPLE
#define BITS_IN_JSAMPLE 8
#endif

#define HAVE_STDDEF_H 1
#define HAVE_STDLIB_H 1
#define NEED_SYS_TYPES_H 1

#undef C_ARITH_CODING_SUPPORTED
#undef D_ARITH_CODING_SUPPORTED
#undef WITH_SIMD
#undef RIGHT_SHIFT_IS_UNSIGNED

#endif
