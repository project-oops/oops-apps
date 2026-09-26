/* Shadows upstream's FreeBSD locale backend, because this target is FreeBSD-flavoured
 * without being FreeBSD.
 *
 * # This applies to BOTH configurations, and that is the correction of 2026-09-24
 *
 * It lived under `include/freestanding/` at first, on the reasoning that a hosted title
 * gets the Mesa sysroot's real `xlocale.h` and so wants upstream's real backend. **The
 * sysroot having the header is not the same as the console having the function.** With
 * the hosted libc++ built against upstream's backend, the CTS's import manifest came
 * back with a dozen names the platform does not export:
 *
 *     asprintf_l  localeconv_l  newlocale  freelocale  snprintf_l  sscanf_l
 *     strcoll_l  strftime_l  strtod_l  strtof_l  strtold_l  strtoll_l
 *     strtoull_l  strxfrm_l  catopen  catgets  catclose
 *
 * Those are a *libc* API, and this platform's libc is the console's, not FreeBSD's. So
 * the choice of backend is about what the target exports, which is the same question in
 * both configurations - and this file belongs in `include/`, which both put on the
 * path.
 *
 * `locale.h`, `nl_types.h` and `runetype.h` stay under `freestanding/`: those genuinely
 * are "does the header exist", and the sysroot's real ones are right for a hosted
 * title.
 *
 * `__locale_dir/locale_base_api.h` picks a backend from the predefined macros, and we
 * compile
 * `-target x86_64-unknown-freebsd`, so it reaches for `support/freebsd.h`. That header
 * wants
 * `<xlocale.h>` and the whole `_l` function family - `strtod_l`, `isupper_l`,
 * `mbrtowc_l` and the rest - which is FreeBSD's per-call locale API. The platform's
 * freestanding C library has none of it, and 21 of libc++'s 45 sources stopped on that
 * one missing header when
 * `_LIBCPP_HAS_LOCALIZATION` was first turned on.
 *
 * **Writing that family was the obvious move and it is the wrong one.** This platform
 * has exactly one locale and will only ever have one. Upstream already ships the
 * backend for that case: `support/fuchsia.h` takes the `no_locale/` character and
 * string-conversion headers, which are the plain non-`_l` functions with the locale
 * argument dropped. So the answer is to pick a different existing backend rather than
 * to implement an API whose whole purpose is switching between locales we do not have.
 *
 * This file is found before upstream's because `oops-libcxx.mk` puts `-I<here>/include`
 * ahead of the checkout, which is the same mechanism that serves `__config_site` and
 * the `mbstate_t` shim. Nothing in the pinned tree is patched.
 *
 * What it still needs from the C library is small and is in `include/locale.h` beside
 * this: `locale_t` and the four functions Fuchsia's `__locale_guard` uses. The "C"
 * locale is a single static object there and `uselocale` cannot fail.
 */
#ifndef _LIBCPP___LOCALE_DIR_SUPPORT_FREEBSD_H
#define _LIBCPP___LOCALE_DIR_SUPPORT_FREEBSD_H

/* `locale.cpp` compiles `std::ctype<char>::classic_table()` to
 * `_DefaultRuneLocale.__runetype` on this target, so the type and the object have to be
 * declared by the time it gets there. This is the header on the path - `<__locale>`
 * includes `locale_base_api.h`, which includes this - so the declaration goes in here
 * rather than being left to a header FreeBSD would have supplied and this SDK does not.
 */
#include <runetype.h>

/*
 * Fuchsia's backend builds `std::locale::category` out of `LC_COLLATE_MASK` and its
 * five siblings. FreeBSD keeps those in `<xlocale.h>` rather than `<locale.h>`, and
 * Fuchsia's file includes only `<clocale>` - so on a hosted build they arrive undefined
 * and every consumer of
 * `<locale>` stops on `use of undeclared identifier 'LC_MESSAGES_MASK'`.
 *
 * `include/freestanding/locale.h` declares them itself, so the freestanding build needs
 * nothing here. `__has_include` is what tells the two apart without a build-system
 * flag: the sysroot has
 * `<xlocale.h>` and this SDK does not.
 */
#if __has_include(<xlocale.h>)
#include <xlocale.h>

/*
 * Fuchsia's backend also calls `::asprintf`, and FreeBSD declares that behind
 * `__BSD_VISIBLE` - which `_XOPEN_SOURCE` turns off, and `_XOPEN_SOURCE` is set for the
 * reasons `gl-cts`'s Makefile gives. So the declaration is made here rather than by
 * relaxing visibility for every translation unit, which would bring back the `u_long`
 * collision it was set to avoid.
 */
#include <stdio.h>
extern "C" int asprintf(char **, const char *, ...);
#endif

#include <__locale_dir/support/fuchsia.h>

#endif /* _LIBCPP___LOCALE_DIR_SUPPORT_FREEBSD_H */
