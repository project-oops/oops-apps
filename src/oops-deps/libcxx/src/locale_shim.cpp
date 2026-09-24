/* The platform side of `<locale.h>`'s locale objects and `<nl_types.h>`'s message catalogues, for
 * a console that has one locale and no catalogues.
 *
 * `include/freestanding/locale.h` and `include/freestanding/nl_types.h` have declared these since
 * they were written, and each says "the definitions are in `locale_shim.cpp`" - a file that did
 * not exist. Nothing noticed, because a payload link ignores unresolved symbols: the gap only
 * becomes an error at the point some title actually *calls* one, and until Extreme Tux Racer
 * reached its link the six that libc++ references had never been asked for.
 *
 * **Those two headers are the specification for this file.** Every behaviour below is stated
 * there, in the prose beside each declaration, and this is the transcription of it. If the two
 * ever disagree, the header is right and this is the bug.
 *
 * # Six names, and why that is not the whole of either header
 *
 * `setlocale` and `localeconv` are declared by `locale.h` and defined by `common/posix/posix.c`
 * (`:249` and `:257`), not here. Defining them in both places is a duplicate-symbol link error,
 * which is how that division was established rather than by reading it.
 *
 * The six that were missing are the ones libc++ *references*: the undefined-symbol check lists
 * what is unresolved **and reached**, so names nothing calls stay invisible however absent they
 * are. `duplocale` below has no caller today and is here because the header promises it - the
 * next title to want it should find it, not another empty file named in a comment.
 *
 * # It is the C locale, and that is an answer rather than a placeholder
 *
 * A console title has no locale environment to read and no way for a user to choose one, so a
 * second locale would be a feature with no source of truth behind it. libc++'s own Fuchsia
 * backend assumes exactly this, which is why `__locale_dir/support/freebsd.h` redirects to it.
 *
 * # Freestanding only
 *
 * Registered beside `rune_table.c` and under its guard, for the reason that file gives: the
 * hosted build links the Mesa sysroot, which carries FreeBSD's real `newlocale` and the rest, and
 * two definitions of one symbol is a duplicate-symbol link error. An avoidable one.
 */
#include <locale.h>
#include <nl_types.h>

/* **At global scope, not in the anonymous namespace below.** `locale.h` declares
 * `typedef struct oops_locale *locale_t`, which in C++ names `::oops_locale`; completing it
 * inside the anonymous namespace defines a *different* type that merely shares a spelling, and
 * `newlocale` then cannot return a pointer to it. */
struct oops_locale {
    /* Never read - `locale_t` is an opaque handle with exactly one valid value. The member is
     * here because C++ has no zero-sized object, and two distinct handles must not be able to
     * share an address. */
    int one;
};

namespace {

/*
 * The one locale object, and the only `locale_t` value that exists.
 *
 * **Its type is taken from `locale_t` rather than named**, because the two configurations spell
 * it differently: `include/freestanding/locale.h` makes it `struct oops_locale *`, and the Mesa
 * sysroot's `<xlocale.h>` makes it `struct _xlocale *`. Naming either one compiles in one build
 * and not the other - which is how this file first met the hosted configuration.
 *
 * Nothing here reads the object. It exists so that `newlocale` has a non-null address to hand
 * back and `uselocale` has something to compare against; a single byte would do, and the type is
 * only chosen so the pointer arithmetic is the compiler's problem rather than a cast.
 */
using oops_locale_object = __typeof__(*(locale_t)0);
oops_locale_object s_the_locale = {};

/* What `uselocale` last installed, so it can return the previous one as it is specified to.
 * `LC_GLOBAL_LOCALE` is the value a thread starts with and means "the global locale" - which
 * here is the same single locale as everything else. */
locale_t s_current = LC_GLOBAL_LOCALE;

} /* namespace */

extern "C" {

/* Hands back the one static object, as the header specifies. It does not ask whether the name is
 * one it could honour: `newlocale` is reached through libc++'s own locale machinery, which asks
 * for "C", and the object it gets back is the only locale this platform has either way. */
locale_t newlocale(int mask, const char *locale, locale_t base) {
    (void)mask;
    (void)locale;
    (void)base;
    return &s_the_locale;
}

locale_t duplocale(locale_t loc) {
    (void)loc;
    return &s_the_locale;
}

/* Does nothing to it - there is one object and it is static, so there is nothing to release, and
 * nothing that would make a second `freelocale` of the same handle a double free. */
void freelocale(locale_t loc) {
    (void)loc;
}

/* Cannot fail. Returns the previously installed locale, which is what a caller saves in order to
 * put things back. A null argument is the query form and installs nothing. */
locale_t uselocale(locale_t loc) {
    const locale_t previous = s_current;
    if (loc) s_current = loc;
    return previous;
}

/* Always fails with `(nl_catd)-1`, the "could not open" value every caller checks: there are no
 * catalogues on this platform and nowhere to read one from. */
nl_catd catopen(const char *name, int oflag) {
    (void)name;
    (void)oflag;
    return (nl_catd)-1;
}

/* Returns the caller's own default text, which is what `catgets` is specified to do when the
 * message is not in the catalogue. A title gets its untranslated string rather than a null. */
char *catgets(nl_catd catd, int set_id, int msg_id, const char *s) {
    (void)catd;
    (void)set_id;
    (void)msg_id;
    return (char *)s;
}

/* -1, because nothing was ever opened. `catopen` only ever returns the failure value, so any
 * descriptor arriving here is invalid, and that is what the standard says to report for one. */
int catclose(nl_catd catd) {
    (void)catd;
    return -1;
}

} /* extern "C" */
