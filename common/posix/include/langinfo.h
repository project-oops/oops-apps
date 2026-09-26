/*
 * `nl_langinfo`, which answers one question honestly and refuses the rest.
 *
 * # What asks
 *
 * `ghc::filesystem` - the `std::filesystem` stand-in Bugdom's Pomme bundles - includes this
 * unconditionally on its BSD branch and calls `nl_langinfo(CODESET)` once, at
 * `filesystem_implementation.hpp:2484`, to decide whether paths are UTF-8 and therefore whether it
 * has to transcode them.
 *
 * # CODESET is "UTF-8", and that is a fact rather than a convenience
 *
 * The SDK's filesystem passes path bytes to the kernel unchanged, and the console's filesystem
 * takes UTF-8. So a caller asking what encoding a path is in gets the right answer, and
 * `ghc::filesystem` correctly skips a transcoding step it does not need. Answering anything else -
 * or the empty string, which is what an unset locale gives on a desktop - would have it convert
 * UTF-8 to UTF-8 through a `wchar_t` round trip.
 *
 * # Everything else returns the empty string
 *
 * The other items are a locale's date format, its month names, its currency symbol. There is no
 * locale here: `common/posix`'s `setlocale` accepts only `"C"`, and the console's own regional
 * settings are not exposed to a payload. The empty string is what POSIX says an unsupported or
 * unset item answers, so a caller gets a case it already handles rather than an invented
 * convention.
 */
#ifndef OOPS_POSIX_LANGINFO_H
#define OOPS_POSIX_LANGINFO_H

#include <locale.h>

typedef int nl_item;

/*
 * The item numbers are FreeBSD's. Only `CODESET` is answered; the rest are defined because a
 * program that names one in a switch should compile, and because a number this header invented
 * would collide with the platform's the day anything else defines them.
 */
#define CODESET      0
#define D_T_FMT      1
#define D_FMT        2
#define T_FMT        3
#define T_FMT_AMPM   4
#define AM_STR       5
#define PM_STR       6
#define RADIXCHAR   50
#define THOUSEP     51
#define YESEXPR     52
#define NOEXPR      53
#define CRNCYSTR    56

#ifdef __cplusplus
extern "C" {
#endif

/* Never NULL: `"UTF-8"` for `CODESET`, `""` for everything else. A caller may not free it. */
char *nl_langinfo(nl_item item);

#ifdef __cplusplus
}
#endif

#endif /* OOPS_POSIX_LANGINFO_H */
