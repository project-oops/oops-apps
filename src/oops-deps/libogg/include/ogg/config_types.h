/*
 * libogg generates this from `config_types.h.in` with four integer widths substituted. The
 * build system that does it is autotools, which we do not run - so here are the four answers
 * for this target, which is LP64 x86-64.
 *
 * Nothing about them is a choice: `ogg_int64_t` has to be 64 bits because the format says so,
 * and these spellings are what `<stdint.h>` calls the same things.
 */
#ifndef __CONFIG_TYPES_H__
#define __CONFIG_TYPES_H__
#include <stdint.h>
typedef int16_t ogg_int16_t;
typedef uint16_t ogg_uint16_t;
typedef int32_t ogg_int32_t;
typedef uint32_t ogg_uint32_t;
typedef int64_t ogg_int64_t;
typedef uint64_t ogg_uint64_t;
#endif
