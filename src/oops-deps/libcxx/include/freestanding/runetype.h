/* `_RuneLocale`, because `std::ctype<char>::classic_table()` is defined in terms of it.
 *
 * On this target `locale.cpp` compiles to one line for that function:
 *
 *     return _DefaultRuneLocale.__runetype;
 *
 * so the type and the object must both be visible when `locale.cpp` is compiled.
 * Without them `locale.cpp` does not build, which means no `std::locale`, no streams,
 * and no GL CTS
 * (`oops-apps#D007`). The object itself is in `src/rune_table.c`, generated.
 *
 * # The layout is transcribed, not designed
 *
 * From FreeBSD's `runetype.h`, staged in this collection at
 * `oops-mesa/toolchain/sysroot/usr/include/runetype.h:40-82`. libc++ reaches
 * `__runetype` by offset, so a struct that merely had a field of that name would read
 * the wrong bytes - the preceding members have to be present and the right size even
 * though nothing here reads them.
 *
 * That is also why this is a transcription with a citation rather than a minimal
 * struct: the minimal version compiles, links, and returns the wrong pointer.
 *
 * # `__rune_t` is spelled locally
 *
 * FreeBSD gets it from `<sys/_types.h>`, which this SDK does not have. It is `int`
 * there, and it is `int` here, named `__oops_rune_t` so that nothing mistakes this for
 * the platform's own declaration if the two ever meet.
 */
#ifndef OOPS_LIBCXX_RUNETYPE_H
#define OOPS_LIBCXX_RUNETYPE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int __oops_rune_t;

typedef struct {
    __oops_rune_t __min;
    __oops_rune_t __max;
    __oops_rune_t __map;
    unsigned long *__types;
} _RuneEntry;

typedef struct {
    int __nranges;
    _RuneEntry *__ranges;
} _RuneRange;

typedef struct {
    char __magic[8];
    char __encoding[32];

    __oops_rune_t (*__sgetrune)(const char *, size_t, char const **);
    int (*__sputrune)(__oops_rune_t, char *, size_t, char **);
    __oops_rune_t __invalid_rune;

    unsigned long __runetype[256];
    __oops_rune_t __maplower[256];
    __oops_rune_t __mapupper[256];

    _RuneRange __runetype_ext;
    _RuneRange __maplower_ext;
    _RuneRange __mapupper_ext;

    void *__variable;
    int __variable_len;
} _RuneLocale;

extern const _RuneLocale _DefaultRuneLocale;
extern const _RuneLocale *_CurrentRuneLocale;

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* OOPS_LIBCXX_RUNETYPE_H */
