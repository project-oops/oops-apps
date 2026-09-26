/*
 * libzip's `config.h`, standing in for the one CMake generates from
 * `upstream/config.h.in`. Each answer is a property of `oops-sdk`'s C library and
 * `common/posix`. Upstream's template order is kept so a bump can be diffed against it.
 *
 * No crypto backend: `zip_file_set_encryption` fails with `ZIP_ER_ENCRNOTSUPP` and an
 * AES-encrypted entry cannot be read. `.o2r` archives are not encrypted.
 *
 * No bzip2, LZMA or zstd: only stored and deflated entries decompress, via `../zlib`.
 */
#ifndef HAD_CONFIG_H
#define HAD_CONFIG_H
#ifndef _HAD_ZIPCONF_H
#include "zipconf.h"
#endif
/* BEGIN DEFINES */

/* `zip_fdopen` needs `fdopen`; `oops-sdk`'s `FILE` is not built around a descriptor. */
/* #undef ENABLE_FDOPEN */

/* The `_`-prefixed family below is MSVC's. This is clang on a FreeBSD target. */
/* #undef HAVE___PROGNAME */
/* #undef HAVE__CLOSE */
/* #undef HAVE__DUP */
/* #undef HAVE__FDOPEN */
/* #undef HAVE__FILENO */
/* #undef HAVE__FSEEKI64 */
/* #undef HAVE__FSTAT64 */
/* #undef HAVE__SETMODE */
/* #undef HAVE__SNPRINTF */
/* #undef HAVE__SNPRINTF_S */
/* #undef HAVE__SNWPRINTF_S */
/* #undef HAVE__STAT64 */
/* #undef HAVE__STRDUP */
/* #undef HAVE__STRICMP */
/* #undef HAVE__STRTOI64 */
/* #undef HAVE__STRTOUI64 */
/* #undef HAVE__UNLINK */

/* #undef HAVE_ARC4RANDOM */
/* Apple's copy-on-write file clone, and Linux's ioctl for the same. */
/* #undef HAVE_CLONEFILE */
/* #undef HAVE_FICLONERANGE */

/* No crypto backend (see the file header). */
/* #undef HAVE_COMMONCRYPTO */
/* #undef HAVE_CRYPTO */
/* #undef HAVE_GNUTLS */
/* #undef HAVE_MBEDTLS */
/* #undef HAVE_OPENSSL */
/* #undef HAVE_WINDOWS_CRYPTO */
/* #undef HAVE_GETSECURITYINFO */

/* `common/posix/posix.c`. */
#define HAVE_FILENO

/* No file permissions to change on this platform. */
/* #undef HAVE_FCHMOD */

/* `oops-sdk/src/system/libc.c`; `off_t` is 64-bit, so these are `fseek`/`ftell`. */
#define HAVE_FSEEKO
#define HAVE_FTELLO

/* A payload has no program name to report. libzip uses it only in an error string. */
/* #undef HAVE_GETPROGNAME */

/* The compression libraries libzip can dispatch to. Only zlib is vendored. */
/* #undef HAVE_LIBBZ2 */
/* #undef HAVE_LIBLZMA */
/* #undef HAVE_LIBZSTD */

/* `oops-sdk/include/libc/time.h`. It answers UTC, so an entry's stored modification
 * time is off by the local offset. */
#define HAVE_LOCALTIME_R
/* #undef HAVE_LOCALTIME_S */

/* The `_s` bounded family is Annex K, which neither clang's headers nor this libc
 * provide. */
/* #undef HAVE_MEMCPY_S */
/* #undef HAVE_SNPRINTF_S */
/* #undef HAVE_STRERROR_S */
/* #undef HAVE_STRERRORLEN_S */
/* #undef HAVE_STRNCPY_S */

/* `mkstemp` is used only when writing an archive, to build the replacement beside the
 * original. Reading does not reach it. */
/* #undef HAVE_MKSTEMP */

/* Text/binary mode on a descriptor, which is a Windows distinction. */
/* #undef HAVE_SETMODE */
/* #undef HAVE_STRICMP */

#define HAVE_SNPRINTF
/* `common/posix/posix.c`, ASCII-only folding - see the comment there. */
#define HAVE_STRCASECMP
#define HAVE_STRDUP
#define HAVE_STRTOLL
#define HAVE_STRTOULL

/* `tm_zone` is a BSD extension; there is no timezone database to name one. */
/* #undef HAVE_STRUCT_TM_TM_ZONE */

/* clang provides `<stdbool.h>` itself; the other two are `common/posix/include`. */
#define HAVE_STDBOOL_H
#define HAVE_STRINGS_H
#define HAVE_UNISTD_H

#define SIZEOF_OFF_T 8
#define SIZEOF_SIZE_T 8

/* `common/posix/include/dirent.h`. The others are the pre-POSIX spellings of it. */
#define HAVE_DIRENT_H
/* #undef HAVE_FTS_H */
/* #undef HAVE_NDIR_H */
/* #undef HAVE_SYS_DIR_H */
/* #undef HAVE_SYS_NDIR_H */

/* x86-64. */
/* #undef WORDS_BIGENDIAN */

/* A payload links one static archive; there is no shared object to build. */
/* #undef HAVE_SHARED */
/* END DEFINES */
#define PACKAGE "libzip"
#define VERSION "1.11.4"

#endif /* HAD_CONFIG_H */
