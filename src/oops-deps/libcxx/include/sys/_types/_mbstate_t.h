/*
 * `mbstate_t`, by the route libc++ asks for it.
 *
 * `__mbstate_t.h` tries four platform headers in order - glibc's `<bits/types/mbstate_t.h>`,
 * Darwin's `<sys/_types/_mbstate_t.h>`, then `<wchar.h>`, then `<uchar.h>` - and `#error`s if
 * none is there. This SDK has none of them, so we answer the second, which is a header whose
 * entire job is this one type.
 *
 * **It matters that this is a type and not a function.** Providing `<wchar.h>` instead would
 * declare `wcslen` and its kin as well, and a caller would then compile against a wide-character
 * library that does not exist and fail at link - or not, since a payload link ignores
 * unresolved symbols. One typedef cannot mislead anyone that way.
 *
 * The layout only has to be big enough and trivially copyable; nothing here interprets it,
 * because the wide-character conversions that would are switched off in `__config_site`.
 */
#ifndef OOPS_LIBCXX_MBSTATE_T_H
#define OOPS_LIBCXX_MBSTATE_T_H
typedef struct {
    unsigned int __state;
    unsigned int __bytes;
} mbstate_t;
#endif
