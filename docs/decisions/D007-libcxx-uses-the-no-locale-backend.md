# D007 - libc++ uses the no-locale backend

**Status:** decided
**Date:** 2026-09-26

libc++ is built with `_LIBCPP_HAS_LOCALIZATION` on and the locale backend upstream uses for
Fuchsia (`no_locale/characters.h`, `no_locale/strtonum.h`). `include/__locale_dir/support/freebsd.h`
shadows upstream's FreeBSD backend ahead of the pinned checkout, `include/locale.h` supplies a
single static "C" `locale_t` and a real `lconv`, `include/nl_types.h` a message-catalogue API
whose `catopen` always fails, and `_DefaultRuneLocale` is a table generated from oops-sdk's
`<ctype.h>` predicates by `tools/gen-rune-table.py`.

**Why:** the GL CTS framework needs streams, and streams need localization. A title has one
locale and no way for anyone to choose another, so the FreeBSD `_l` family the default backend
wants would express a choice nothing can make. C library facilities libc++ also needed
(`MB_CUR_MAX`, `asprintf`, `strcoll`, `ungetc` and the rest) went to oops-sdk, where the C
standard puts them.

**Rejected:**
- Writing the `_l` family: several dozen wrappers for a second locale that cannot exist.
- Patching the pinned tree: a shadow header needs no rebase.
- Localization off: the CTS framework does not build without streams.
