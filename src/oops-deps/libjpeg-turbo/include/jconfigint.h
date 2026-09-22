/*
 * libjpeg-turbo's *internal* build configuration, the other half CMake generates.
 *
 * `THREAD_LOCAL` is empty on purpose. It decorates one error-handling buffer so two threads
 * decoding at once do not share it; `oops-sdk` has threads but this build has no `__thread`
 * support wired up, and a title decoding JPEGs from two threads at once is not a thing any
 * title here does. Empty means "one shared buffer", which is what a single-threaded caller has
 * anyway.
 */
#ifndef OOPS_JCONFIGINT_H
#define OOPS_JCONFIGINT_H

#define BUILD "oops"
#define HIDDEN __attribute__((visibility("hidden")))
#define INLINE __inline__ __attribute__((always_inline))
#define THREAD_LOCAL
#define CMAKE_PROJECT_NAME "libjpeg-turbo"
#define VERSION "3.1.4"
#define SIZEOF_SIZE_T 8
#define HAVE_BUILTIN_CTZL 1

/*
 * `FALLTHROUGH` marks a deliberate fall-through between switch cases so the compiler does not
 * warn about it. Upstream's template picks the spelling by compiler; clang has the attribute.
 * It is a *statement* macro - the trailing semicolon is upstream's, not a mistake here.
 */
#define FALLTHROUGH __attribute__((fallthrough));

#undef WITH_SIMD
#undef C_ARITH_CODING_SUPPORTED
#undef D_ARITH_CODING_SUPPORTED

#endif
