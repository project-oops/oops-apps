/*
 * OpenAL Soft's `config.h`, standing in for the one its CMake generates from
 * `config.h.in` by probing the build machine. Every switch in upstream's template is
 * listed, set or unset.
 */

/* Off: the EAX extensions are a Windows-era API nothing here calls. */
/* #undef ALSOFT_EAX */

/* Off: the embedded HRTF data set is for headphone spatialisation. */
/* #undef ALSOFT_EMBED_HRTF_DATA */

/* Both unset, so `almalloc.cpp` over-allocates with `malloc` and aligns by hand. */
/* #undef HAVE_POSIX_MEMALIGN */
/* #undef HAVE__ALIGNED_MALLOC */
/* #undef HAVE_PROC_PIDPATH */
/* #undef HAVE_GETOPT */
/* #undef HAVE_RTKIT */

/* The mixer's SIMD paths compiled; `cpuid` picks one at run time. The console's Zen 2
 * CPU has all four. */
#define HAVE_SSE
#define HAVE_SSE2
#define HAVE_SSE3
#define HAVE_SSE4_1
/* #undef HAVE_NEON */

/* One output backend: SDL2. Upstream always adds null and loopback as well. */
/* #undef HAVE_ALSA */
/* #undef HAVE_OSS */
/* #undef HAVE_PIPEWIRE */
/* #undef HAVE_SOLARIS */
/* #undef HAVE_SNDIO */
/* #undef HAVE_WASAPI */
/* #undef HAVE_DSOUND */
/* #undef HAVE_WINMM */
/* #undef HAVE_PORTAUDIO */
/* #undef HAVE_PULSEAUDIO */
/* #undef HAVE_JACK */
/* #undef HAVE_COREAUDIO */
/* #undef HAVE_OPENSL */
/* #undef HAVE_OBOE */
/* #undef HAVE_WAVE */
#define HAVE_SDL2

/* Unset: a payload links every backend in, so `dynload.cpp` has nothing to `dlopen`. */
/* #undef HAVE_DLFCN_H */
/* #undef HAVE_PTHREAD_NP_H */
/* #undef HAVE_MALLOC_H */

/* `cpuid.h` is one of clang's own headers. */
#define HAVE_CPUID_H
/* #undef HAVE_INTRIN_H */
/* #undef HAVE_GUIDDEF_H */
/* #undef HAVE_INITGUID_H */
#define HAVE_GCC_GET_CPUID
/* #undef HAVE_CPUID_INTRINSIC */
#define HAVE_SSE_INTRINSICS

/* Unset: the SDK's `<pthread.h>` has no priority or thread-name calls. The mixer runs
 * in SDL's audio thread, whose priority SDL chooses. */
/* #undef HAVE_PTHREAD_SETSCHEDPARAM */
/* #undef HAVE_PTHREAD_SETNAME_NP */
/* #undef HAVE_PTHREAD_SET_NAME_NP */

/* Where `alsoftrc` and HRTF data sets would be searched for. Neither ships. */
/* #undef ALSOFT_INSTALL_DATADIR */
