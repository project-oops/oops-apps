/*
 * libjpeg-turbo's public build configuration. CMake normally generates it from
 * `src/jconfig.h.in`; we do not run its CMake, so every value here is a decision rather
 * than whatever the build machine happened to detect.
 *
 * `JPEG_LIB_VERSION 62` is the ABI a program written against the original IJG libjpeg
 * expects, which is what Neverball is - it includes `<jpeglib.h>` and calls
 * `jpeg_read_header`.
 *
 * **Arithmetic coding is off and SIMD is off.** Arithmetic coding is patent-era baggage
 * that almost no JPEG in the wild uses; SIMD is per-architecture assembly built through
 * NASM, and the C path decodes the same images. A title that measures JPEG decode as
 * its bottleneck can revisit the second of those with a number in hand.
 */
#ifndef OOPS_JCONFIG_H
#define OOPS_JCONFIG_H

#define JPEG_LIB_VERSION 62
#define LIBJPEG_TURBO_VERSION 3.1.4
#define LIBJPEG_TURBO_VERSION_NUMBER 3001004

#define MEM_SRCDST_SUPPORTED 1

/*
 * **The `#ifndef` is load-bearing and was missing here at first.**
 *
 * libjpeg-turbo 3.x compiles a dozen of its sources three times over, at 8, 12 and 16
 * bits per sample, through generated one-line wrappers that `#define BITS_IN_JSAMPLE`
 * and include the real file. `jmorecfg.h` renames every function accordingly -
 * `jinit_1pass_quantizer` becomes `j12init_1pass_quantizer` - and `jdmaster.c` calls
 * whichever the image's precision needs, at run time.
 *
 * Writing this unguarded overrode the wrapper's own define, so all three passes
 * compiled as 8-bit, every `j12*` symbol went undefined, and the library did not link.
 * Upstream's `jconfig.h.in` has the guard; dropping it was a transcription error, and
 * it cost a link that had looked like 27 clean compiles.
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
