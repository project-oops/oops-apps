/*
 * `sys/endian.h` - FreeBSD's byte-order conversions. PhysFS and Luanti include it on
 * FreeBSD, which is the ABI this target is.
 *
 * The target is little-endian x86-64, so the `le` forms are identities and the `be` forms
 * swap, through the compiler's builtins.
 */
#ifndef OOPS_POSIX_SYS_ENDIAN_H
#define OOPS_POSIX_SYS_ENDIAN_H

#include <stdint.h>

#ifndef _LITTLE_ENDIAN
#define _LITTLE_ENDIAN 1234
#define _BIG_ENDIAN 4321
#define _PDP_ENDIAN 3412
#define _BYTE_ORDER _LITTLE_ENDIAN
#endif
#ifndef LITTLE_ENDIAN
#define LITTLE_ENDIAN _LITTLE_ENDIAN
#define BIG_ENDIAN _BIG_ENDIAN
#define PDP_ENDIAN _PDP_ENDIAN
#define BYTE_ORDER _BYTE_ORDER
#endif

#define bswap16(x) __builtin_bswap16((uint16_t)(x))
#define bswap32(x) __builtin_bswap32((uint32_t)(x))
#define bswap64(x) __builtin_bswap64((uint64_t)(x))

#define htobe16(x) bswap16(x)
#define htobe32(x) bswap32(x)
#define htobe64(x) bswap64(x)
#define htole16(x) ((uint16_t)(x))
#define htole32(x) ((uint32_t)(x))
#define htole64(x) ((uint64_t)(x))
#define be16toh(x) bswap16(x)
#define be32toh(x) bswap32(x)
#define be64toh(x) bswap64(x)
#define le16toh(x) ((uint16_t)(x))
#define le32toh(x) ((uint32_t)(x))
#define le64toh(x) ((uint64_t)(x))

#endif /* OOPS_POSIX_SYS_ENDIAN_H */
