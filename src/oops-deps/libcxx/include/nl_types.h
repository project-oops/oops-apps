/* Message catalogues, for a platform that has none.
 *
 * libc++'s `std::messages` facet is specified in terms of X/Open message catalogues - files of
 * translated strings opened by name and indexed by set and message number. A console title
 * ships no catalogues and has nowhere to read one from, so `catopen` always fails and
 * `std::messages::open` reports "no catalogue", which is the honest answer and the one the
 * facet is designed to give.
 *
 * The declarations still have to exist, because `<__locale_dir/messages.h>` is compiled
 * whenever localization is on, whether or not a program ever asks for a message. The
 * definitions are in `locale_shim.cpp`.
 *
 * `nl_catd` is `void *` rather than an opaque struct pointer because that is what FreeBSD and
 * glibc both use, and a program that stored one in a `void *` should keep compiling.
 */
#ifndef OOPS_LIBCXX_NL_TYPES_H
#define OOPS_LIBCXX_NL_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#define NL_SETD   1
#define NL_CAT_LOCALE 1

typedef void *nl_catd;
typedef int   nl_item;

/* Always fails with `(nl_catd)-1`, which is the "could not open" value every caller checks. */
nl_catd catopen(const char *name, int oflag);

/* Returns `s`, the caller's own default text. That is what `catgets` is specified to do when
   the message is not in the catalogue, so a title gets its untranslated string rather than a
   null or an empty one. */
char *catgets(nl_catd catd, int set_id, int msg_id, const char *s);

int catclose(nl_catd catd);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* OOPS_LIBCXX_NL_TYPES_H */
