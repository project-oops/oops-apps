/* Shadows upstream's FreeBSD locale backend, because this target is FreeBSD-flavoured without
 * being FreeBSD.
 *
 * `__locale_dir/locale_base_api.h` picks a backend from the predefined macros, and we compile
 * `-target x86_64-unknown-freebsd`, so it reaches for `support/freebsd.h`. That header wants
 * `<xlocale.h>` and the whole `_l` function family - `strtod_l`, `isupper_l`, `mbrtowc_l` and
 * the rest - which is FreeBSD's per-call locale API. The platform's freestanding C library has
 * none of it, and 21 of libc++'s 45 sources stopped on that one missing header when
 * `_LIBCPP_HAS_LOCALIZATION` was first turned on.
 *
 * **Writing that family was the obvious move and it is the wrong one.** This platform has
 * exactly one locale and will only ever have one. Upstream already ships the backend for that
 * case: `support/fuchsia.h` takes the `no_locale/` character and string-conversion headers,
 * which are the plain non-`_l` functions with the locale argument dropped. So the answer is to
 * pick a different existing backend rather than to implement an API whose whole purpose is
 * switching between locales we do not have.
 *
 * This file is found before upstream's because `oops-libcxx.mk` puts `-I<here>/include` ahead of
 * the checkout, which is the same mechanism that serves `__config_site` and the `mbstate_t`
 * shim. Nothing in the pinned tree is patched.
 *
 * What it still needs from the C library is small and is in `include/locale.h` beside this:
 * `locale_t` and the four functions Fuchsia's `__locale_guard` uses. The "C" locale is a single
 * static object there and `uselocale` cannot fail.
 */
#ifndef _LIBCPP___LOCALE_DIR_SUPPORT_FREEBSD_H
#define _LIBCPP___LOCALE_DIR_SUPPORT_FREEBSD_H

/* `locale.cpp` compiles `std::ctype<char>::classic_table()` to `_DefaultRuneLocale.__runetype`
 * on this target, so the type and the object have to be declared by the time it gets there.
 * This is the header on the path - `<__locale>` includes `locale_base_api.h`, which includes
 * this - so the declaration goes in here rather than being left to a header FreeBSD would have
 * supplied and this SDK does not. */
#include <runetype.h>

#include <__locale_dir/support/fuchsia.h>

#endif /* _LIBCPP___LOCALE_DIR_SUPPORT_FREEBSD_H */
