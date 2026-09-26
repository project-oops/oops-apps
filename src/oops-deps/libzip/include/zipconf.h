/*
 * `zipconf.h` - ours, standing in for the one CMake generates from
 * `upstream/zipconf.h.in`.
 *
 * It is a public header: `zip.h` includes it, so anything using libzip sees these
 * types. CMake fills the template by *testing* for an integer type of each width and
 * falling back through `long long`, `__int64` and so on. None of that search is a
 * decision here - this target is LP64 with a working `<stdint.h>`, so the exact-width
 * typedefs are the answer and there is nothing to probe. The version numbers are the
 * pinned tag in `../upstream.lock`; a bump moves both.
 *
 * `ZIP_STATIC` is not defined. It only guards `__declspec(dllimport)` on Windows.
 */
#ifndef _HAD_ZIPCONF_H
#define _HAD_ZIPCONF_H

#define LIBZIP_VERSION "1.11.4"
#define LIBZIP_VERSION_MAJOR 1
#define LIBZIP_VERSION_MINOR 11
#define LIBZIP_VERSION_MICRO 4

#include <stdint.h>

typedef int8_t zip_int8_t;
typedef uint8_t zip_uint8_t;
typedef int16_t zip_int16_t;
typedef uint16_t zip_uint16_t;
typedef int32_t zip_int32_t;
typedef uint32_t zip_uint32_t;
typedef int64_t zip_int64_t;
typedef uint64_t zip_uint64_t;

#define ZIP_INT8_MIN (-ZIP_INT8_MAX - 1)
#define ZIP_INT8_MAX 0x7f
#define ZIP_UINT8_MAX 0xff

#define ZIP_INT16_MIN (-ZIP_INT16_MAX - 1)
#define ZIP_INT16_MAX 0x7fff
#define ZIP_UINT16_MAX 0xffff

#define ZIP_INT32_MIN (-ZIP_INT32_MAX - 1L)
#define ZIP_INT32_MAX 0x7fffffffL
#define ZIP_UINT32_MAX 0xffffffffLU

#define ZIP_INT64_MIN (-ZIP_INT64_MAX - 1LL)
#define ZIP_INT64_MAX 0x7fffffffffffffffLL
#define ZIP_UINT64_MAX 0xffffffffffffffffULL

#endif /* zipconf.h */
