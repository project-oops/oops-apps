/*
 * cURL's build configuration for this target - what `lib/curl_config-cmake.h.in` would
 * say after CMake had probed it. Each `HAVE_` below is something `common/posix` or
 * oops-sdk's C library declares and defines; what is absent is absent on purpose, with
 * the reason beside it.
 *
 * The build is HTTP and HTTPS through Mbed TLS, IPv4, with zlib content decoding: what
 * SuperTuxKart's add-on and news fetches and NetSurf's page loads use.
 * `OOPS_CURL_HOSTED` is set by `oops-curl.mk` for a title built against the Mesa
 * sysroot, whose FreeBSD C library has the few calls the freestanding layer refuses.
 */
#ifndef OOPS_CURL_CONFIG_H
#define OOPS_CURL_CONFIG_H

#define CURL_OS "x86_64-unknown-freebsd"

/* --- protocols: HTTP and HTTPS --- */
#define CURL_DISABLE_DICT 1
#define CURL_DISABLE_FILE 1 /* NetSurf reads file: URLs itself */
#define CURL_DISABLE_FTP 1
#define CURL_DISABLE_GOPHER 1
#define CURL_DISABLE_IMAP 1
#define CURL_DISABLE_LDAP 1
#define CURL_DISABLE_LDAPS 1
#define CURL_DISABLE_MQTT 1
#define CURL_DISABLE_POP3 1
#define CURL_DISABLE_RTSP 1
#define CURL_DISABLE_SMTP 1
#define CURL_DISABLE_TELNET 1
#define CURL_DISABLE_TFTP 1
#define CURL_DISABLE_IPFS 1
#define CURL_DISABLE_WEBSOCKETS 1
/* Authentication that needs a system library this target does not have. */
#define CURL_DISABLE_KERBEROS_AUTH 1
#define CURL_DISABLE_NEGOTIATE_AUTH 1
#define CURL_DISABLE_AWS 1
/* No `~/.netrc` to read, and no DNS-over-HTTPS resolver to point at. */
#define CURL_DISABLE_NETRC 1
#define CURL_DISABLE_DOH 1
/* The wake-up pair a multi handle makes for `curl_multi_wakeup`: `socketpair` does not
 * exist in the freestanding layer, and neither title wakes a multi handle from another
 * thread. */
#define CURL_DISABLE_SOCKETPAIR 1

/* --- TLS --- */
#define USE_MBEDTLS 1
/* No system certificate store: a title names its CA bundle with `CURLOPT_CAINFO`. */
#define CURL_DISABLE_CA_SEARCH 1

#define HAVE_LIBZ 1

/* --- types and sizes --- */
#define SIZEOF_INT 4
#define SIZEOF_LONG 8
#define SIZEOF_OFF_T 8
#define SIZEOF_CURL_OFF_T 8
#define SIZEOF_CURL_SOCKET_T 4
#define SIZEOF_SIZE_T 8
#define SIZEOF_TIME_T 8
#define HAVE_BOOL_T 1
#define HAVE_STDBOOL_H 1
#define HAVE_STRUCT_TIMEVAL 1
#define HAVE_SUSECONDS_T 1
#define HAVE_SA_FAMILY_T 1
#define HAVE_STRUCT_SOCKADDR_STORAGE 1
#define HAVE_ATOMIC 1
#define HAVE_STDATOMIC_H 1

/* --- headers --- */
#define HAVE_ARPA_INET_H 1
#define HAVE_FCNTL_H 1
#define HAVE_LIBGEN_H 1
#define HAVE_LOCALE_H 1
#define HAVE_NETDB_H 1
#define HAVE_NETINET_IN_H 1
#define HAVE_NETINET_TCP_H 1
#define HAVE_POLL_H 1
#define HAVE_STRINGS_H 1
#define HAVE_SYS_IOCTL_H 1
#define HAVE_SYS_PARAM_H 1
#define HAVE_SYS_POLL_H 1
#define HAVE_SYS_SELECT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_UNISTD_H 1

/* --- calls --- */
#define HAVE_SOCKET 1
#define HAVE_RECV 1
#define HAVE_SEND 1
#define HAVE_POLL 1
#define HAVE_FCNTL 1
/* `common/posix`'s fcntl sets the platform's SO_NBIO. */
#define HAVE_FCNTL_O_NONBLOCK 1
#define HAVE_GETADDRINFO 1
#define HAVE_FREEADDRINFO 1
#define HAVE_CLOCK_GETTIME_MONOTONIC 1
#define HAVE_GETTIMEOFDAY 1
#define HAVE_GMTIME_R 1
#define HAVE_STRCASECMP 1
#define HAVE_SIGNAL 1
#define HAVE_ARC4RANDOM 1
#define HAVE_BASENAME 1

#if defined(OOPS_CURL_HOSTED)
/* FreeBSD's C library: the socket queries and a resolver thread. */
#define HAVE_GETSOCKNAME 1
#define HAVE_GETPEERNAME 1
#define HAVE_THREADS_POSIX 1
#define USE_RESOLV_THREADED 1
#define HAVE_GETADDRINFO_THREADSAFE 1
#else
/* `getsockname` and `getpeername` refuse in `common/posix` - the SDK cannot answer them
 * - so the connection's local and peer addresses go unreported rather than failing a
 * transfer. The resolver is synchronous: `getaddrinfo` runs on the calling thread. */
#endif

#endif /* OOPS_CURL_CONFIG_H */
