/*
 * Webview build glue: the last few C++ runtime symbols the prebuilt freestanding libc++/libc++abi
 * archives do not carry, provided here so the payload links cleanly (a payload link ignores
 * unresolved symbols, so an undefined one would fault on the console instead of failing the build).
 *
 * Filed for the SDK to fold into its libc++ build / freestanding libc (REQ-20260925T0110Z-e1d9);
 * this file goes away when it does.
 */

#include <locale.h>

/* NB: the typeinfos for std::bad_weak_ptr / bad_function_call / bad_variant_access and the internal
 * std::__shared_weak_count are absent from the prebuilt libc++/libc++abi archives (their
 * memory/functional/variant translation units were not built in). They are referenced by
 * shared_ptr / std::function / std::variant RTTI but only *read* on exceptional paths a normal HTML
 * render does not take, so they are allow-listed at the payload link (EXTRA_UNDEF_ALLOW in the
 * Makefile) rather than defined here - a throw of one of them would still fault, which is a known
 * limitation until the SDK builds libc++ with those TUs (REQ-20260925T0110Z-e1d9). */

/* libc++'s number formatting calls localeconv()/setlocale(); the SDK's freestanding libc has no
 * locale. The "C" locale is the right answer here: a '.' decimal point and no grouping. */
extern "C" {

struct lconv *localeconv(void) {
    static struct lconv __lc;
    static char __dot[] = ".";
    static char __empty[] = "";
    __lc.decimal_point = __dot;
    __lc.thousands_sep = __empty;
    __lc.grouping = __empty;
    return &__lc;
}

char *setlocale(int __category, const char *__locale) {
    (void)__category; (void)__locale;
    static char __c[] = "C";
    return __c;
}

} /* extern "C" */
