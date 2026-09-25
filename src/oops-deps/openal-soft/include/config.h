/*
 * OpenAL Soft's `config.h`, which its CMake generates from `config.h.in` by probing the build
 * machine. Written by hand because nothing here runs CMake - and because probing the *build*
 * machine is exactly the wrong question for a console target.
 *
 * Every switch upstream's template has is listed, set or deliberately unset, so a reader can see
 * the whole decision rather than infer it from what is missing.
 */

/* Off: the EAX extensions are a Windows-era API nothing here calls. */
/* #undef ALSOFT_EAX */

/* **Off**: embedding bakes a 44.1 kHz HRTF data set into the library for headphone
 * spatialisation. SuperTux and SuperTuxKart play through the console's output, not an HRTF. */
/* #undef ALSOFT_EMBED_HRTF_DATA */

/* Both unset, so `almalloc.cpp` over-allocates with `malloc` and aligns by hand - its portable
 * arm, and the one that asks the libc for nothing beyond `malloc` and `free`. */
/* #undef HAVE_POSIX_MEMALIGN */
/* #undef HAVE__ALIGNED_MALLOC */
/* #undef HAVE_PROC_PIDPATH */
/* #undef HAVE_GETOPT */
/* #undef HAVE_RTKIT */

/* **The mixer's SIMD paths.** The console's CPU is Zen 2, which has all four, and upstream picks
 * the fastest one at run time from `cpuid` - these say which were compiled, not which run. */
#define HAVE_SSE
#define HAVE_SSE2
#define HAVE_SSE3
#define HAVE_SSE4_1
/* #undef HAVE_NEON */

/* **One output backend: SDL2.** Upstream always adds null and loopback as well; everything else
 * here names an audio system this target does not have. */
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

/* Unset: `dynload.cpp` would `dlopen` backend libraries, and a payload links everything in. */
/* #undef HAVE_DLFCN_H */
/* #undef HAVE_PTHREAD_NP_H */
/* #undef HAVE_MALLOC_H */

/* `cpuid.h` is one of clang's own headers, so it is present whatever the libc. */
#define HAVE_CPUID_H
/* #undef HAVE_INTRIN_H */
/* #undef HAVE_GUIDDEF_H */
/* #undef HAVE_INITGUID_H */
#define HAVE_GCC_GET_CPUID
/* #undef HAVE_CPUID_INTRINSIC */
#define HAVE_SSE_INTRINSICS

/* Unset: real-time priority and thread names go through pthread calls the SDK's `<pthread.h>`
 * does not have. The mixer runs in SDL's audio thread, whose priority SDL already chose. */
/* #undef HAVE_PTHREAD_SETSCHEDPARAM */
/* #undef HAVE_PTHREAD_SETNAME_NP */
/* #undef HAVE_PTHREAD_SET_NAME_NP */

/* Where `alsoftrc` and HRTF data sets would be searched for. Neither ships. */
/* #undef ALSOFT_INSTALL_DATADIR */
