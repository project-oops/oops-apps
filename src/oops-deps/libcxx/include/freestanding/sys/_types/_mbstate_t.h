/*
 * `mbstate_t`, by the route libc++ asks for it.
 *
 * `__mbstate_t.h` tries four platform headers in order - glibc's
 * `<bits/types/mbstate_t.h>`, Darwin's `<sys/_types/_mbstate_t.h>`, then `<wchar.h>`,
 * then `<uchar.h>` - and `#error`s if none is there. This SDK has none of them, so we
 * answer the second, which is a header whose entire job is this one type.
 *
 * **It matters that this is a type and not a function.** Providing `<wchar.h>` instead
 * would declare `wcslen` and its kin as well, and a caller would then compile against a
 * wide-character library that does not exist and fail at link - or not, since a payload
 * link ignores unresolved symbols. One typedef cannot mislead anyone that way.
 *
 * The layout only has to be big enough and trivially copyable; nothing here interprets
 * it, because the wide-character conversions that would are switched off in
 * `__config_site`.
 *
 * # The `_MBSTATE_T_DECLARED` guard, which is not this file's own
 *
 * **oops-sdk declares this type too**, in `include/libc/wchar.h`, which it gained on
 * 2026-09-23 for an unrelated reason: libc++'s `<cwchar>` needs `struct tm` to have
 * been declared before
 * `<ctime>` gets to it, and C puts that declaration in `<wchar.h>`. The argument above
 * - that a
 * `<wchar.h>` would bring functions with it - is why *this* file exists and still
 * holds; the one over there deliberately declares no functions at all.
 *
 * So two headers now define `mbstate_t` and a translation unit can see either first.
 * They share FreeBSD's own guard name rather than each keeping a private one, which
 * makes the second inclusion a no-op instead of a redefinition. **Both bodies must stay
 * identical**: when they were not, 21 of libc++'s 45 sources failed with `typedef
 * redefinition with different types` and nothing in the message named either file.
 */
#ifndef OOPS_LIBCXX_MBSTATE_T_H
#define OOPS_LIBCXX_MBSTATE_T_H

#ifndef _MBSTATE_T_DECLARED
#define _MBSTATE_T_DECLARED
typedef struct {
    unsigned int __state;
    unsigned int __bytes;
} mbstate_t;
#endif

#endif
