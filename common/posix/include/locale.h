/*
 * One locale exists here, and it is "C".
 *
 * A locale needs a database of collation orders, number formats and month names, and a setting
 * that says which one to use. This console hands a payload neither, so `setlocale` reports the
 * only locale there is rather than failing - a program asking for "en_US.UTF-8" is asking to
 * format numbers its own way, and telling it no would make it fall over rather than fall back.
 *
 * `localeconv` answers the C locale's own values: a full stop for the decimal point and nothing
 * for grouping, which is what a program parsing its own data files should already assume.
 */
#ifndef OOPS_POSIX_LOCALE_H
#define OOPS_POSIX_LOCALE_H

#define LC_ALL      0
#define LC_COLLATE  1
#define LC_CTYPE    2
#define LC_MONETARY 3
#define LC_NUMERIC  4
#define LC_TIME     5
#define LC_MESSAGES 6

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
};

#ifdef __cplusplus
extern "C" {
#endif
char *setlocale(int category, const char *locale);
struct lconv *localeconv(void);
#ifdef __cplusplus
}
#endif

#endif
