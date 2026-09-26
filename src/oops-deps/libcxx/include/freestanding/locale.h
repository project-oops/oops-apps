/* The C library's `<locale.h>`, for a platform that has exactly one locale.
 *
 * oops-sdk's freestanding C library has no `locale.h` at all, and libc++'s `<clocale>`
 * includes one. This supplies it, here rather than in oops-sdk, for the same reason
 * `__config_site` and the `mbstate_t` shim live here: it is libc++'s platform glue,
 * shaped by what libc++ asks for, and a freestanding C library is right not to carry
 * it.
 *
 * # One locale, and it is "C"
 *
 * Everything below is the C locale and cannot be anything else. `setlocale` accepts the
 * names that mean it and refuses the rest by returning null, which is what the standard
 * says to do for a locale that cannot be selected - not a stub pretending to succeed.
 * `newlocale` hands back the one static object, `freelocale` does nothing to it, and
 * `uselocale` cannot fail.
 *
 * That is not a placeholder for a real implementation. A console title has no locale
 * environment to read and no way for a user to choose one, so a second locale would be
 * a feature with no source of truth behind it. libc++'s own Fuchsia backend is built on
 * the same assumption, which is why `__locale_dir/support/freebsd.h` beside this file
 * redirects to it.
 *
 * # Why `lconv` still has real contents
 *
 * `localeconv` is read by `std::numpunct` and by `printf`-family formatting, and
 * returning null or a zeroed struct makes number formatting silently produce the wrong
 * thing rather than fail. The values below are the C locale's, exactly as the standard
 * specifies it: decimal point ".", everything else empty or `CHAR_MAX` for "not
 * available in this locale".
 */
#ifndef OOPS_LIBCXX_LOCALE_H
#define OOPS_LIBCXX_LOCALE_H

#include <limits.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LC_ALL 0
#define LC_COLLATE 1
#define LC_CTYPE 2
#define LC_MONETARY 3
#define LC_NUMERIC 4
#define LC_TIME 5
#define LC_MESSAGES 6

/* The mask spellings `newlocale` takes. Values are FreeBSD's, so a caller that
   hardcoded one from a header it read elsewhere agrees with this. */
#define LC_COLLATE_MASK ((int)0x00000001)
#define LC_CTYPE_MASK ((int)0x00000002)
#define LC_MONETARY_MASK ((int)0x00000004)
#define LC_NUMERIC_MASK ((int)0x00000008)
#define LC_TIME_MASK ((int)0x00000010)
#define LC_MESSAGES_MASK ((int)0x00000020)
#define LC_ALL_MASK                                                                    \
    (LC_COLLATE_MASK | LC_CTYPE_MASK | LC_MONETARY_MASK | LC_NUMERIC_MASK |            \
     LC_TIME_MASK | LC_MESSAGES_MASK)

struct lconv {
    char *decimal_point;
    char *thousands_sep;
    char *grouping;
    char *int_curr_symbol;
    char *currency_symbol;
    char *mon_decimal_point;
    char *mon_thousands_sep;
    char *mon_grouping;
    char *positive_sign;
    char *negative_sign;
    char int_frac_digits;
    char frac_digits;
    char p_cs_precedes;
    char p_sep_by_space;
    char n_cs_precedes;
    char n_sep_by_space;
    char p_sign_posn;
    char n_sign_posn;
    char int_p_cs_precedes;
    char int_n_cs_precedes;
    char int_p_sep_by_space;
    char int_n_sep_by_space;
    char int_p_sign_posn;
    char int_n_sign_posn;
};

char *setlocale(int category, const char *locale);
struct lconv *localeconv(void);

/* `locale_t` is an opaque handle. It has exactly one valid value, which `newlocale` and
   `duplocale` both return and which `uselocale` always installs. */
typedef struct oops_locale *locale_t;

#define LC_GLOBAL_LOCALE ((locale_t) - 1)

locale_t newlocale(int mask, const char *locale, locale_t base);
locale_t duplocale(locale_t loc);
void freelocale(locale_t loc);
locale_t uselocale(locale_t loc);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* OOPS_LIBCXX_LOCALE_H */
