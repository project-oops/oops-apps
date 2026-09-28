/*
 * Mbed TLS's user configuration for this target: `MBEDTLS_USER_CONFIG_FILE`, read after
 * upstream's own `mbedtls_config.h`, so everything not named here is upstream's
 * default.
 */
#ifndef OOPS_MBEDTLS_CONFIG_H
#define OOPS_MBEDTLS_CONFIG_H

/* Entropy. Upstream's platform source reads `getrandom` through a FreeBSD version
 * test, or `/dev/urandom`, neither of which this target answers the way a FreeBSD
 * userland does. The hardware-poll hook is the documented replacement:
 * `oops_platform.c` fills it from `getentropy`, which is the kernel's getrandom. */
#define MBEDTLS_NO_PLATFORM_ENTROPY
#define MBEDTLS_ENTROPY_HARDWARE_ALT

/* The millisecond clock. Upstream picks its POSIX implementation by `_POSIX_VERSION`,
 * which the freestanding POSIX layer does not define; `oops_platform.c` reads the same
 * monotonic clock. */
#define MBEDTLS_PLATFORM_MS_TIME_ALT

/* The TCP and timer modules are for programs that let Mbed TLS own the socket. curl
 * owns its own and hands Mbed TLS send and receive callbacks, so neither is built. */
#undef MBEDTLS_NET_C
#undef MBEDTLS_TIMING_C

/* PSA's key store and the global entropy state are shared across threads, so a program
 * that runs TLS from more than one needs these locks. The hosted build has them, over
 * FreeBSD's pthreads: SuperTuxKart fetches add-ons and news on a thread of their own.
 * The freestanding build does not, because upstream's pthread arm initialises its
 * global mutexes statically and oops-sdk's `pthread.h` leaves
 * `PTHREAD_MUTEX_INITIALIZER` undefined on purpose - a mutex there is a kernel handle
 * `pthread_mutex_init` creates. A freestanding title that runs TLS on one thread
 * (NetSurf's fetch loop) needs no locks; one that does not would need
 * `MBEDTLS_THREADING_ALT` and an explicit set-up call. `oops-mbedtls.mk` sets the
 * switch. */
#if defined(OOPS_MBEDTLS_THREADS)
#define MBEDTLS_THREADING_C
#define MBEDTLS_THREADING_PTHREAD
#endif

#endif /* OOPS_MBEDTLS_CONFIG_H */
