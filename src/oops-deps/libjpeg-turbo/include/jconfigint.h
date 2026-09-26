/*
 * libjpeg-turbo's internal build configuration, the other half CMake generates.
 *
 * `THREAD_LOCAL` is empty: this build has no `__thread` support, so the one
 * error-handling buffer it decorates is shared. No title decodes JPEGs from two
 * threads at once.
 */
#ifndef OOPS_JCONFIGINT_H
#define OOPS_JCONFIGINT_H

#define BUILD "oops"
#define HIDDEN __attribute__((visibility("hidden")))
#define INLINE __inline__ __attribute__((always_inline))
#define THREAD_LOCAL
/* Upstream's template substitutes `@CMAKE_PROJECT_NAME@`; read by `jcmaster.c:800`. */
#define PACKAGE_NAME "libjpeg-turbo"
#define VERSION "3.1.4"
#define SIZEOF_SIZE_T 8
#define HAVE_BUILTIN_CTZL 1

/*
 * Upstream's template picks the fall-through spelling by compiler; clang has the
 * attribute. It is a statement macro, so the trailing semicolon is upstream's.
 */
#define FALLTHROUGH __attribute__((fallthrough));

#undef WITH_SIMD
#undef C_ARITH_CODING_SUPPORTED
#undef D_ARITH_CODING_SUPPORTED

#endif
