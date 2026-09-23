# D007 - The CTS blocker is localization, and libc++ picks a different backend rather than growing an `_l` family

**decided** · 2026-09-23 (measured by compiling all 45 libc++ sources for the target, twice, and
by counting the standard-library headers the CTS framework includes)

`oops-mesa` unit 8 is "a bounded, named subset of the GL 3.3 CTS run on the hardware". With
`cxx-throw` passing 5/5 ([D006](D006-three-things-had-to-be-true-for-a-throw-to-reach-its-catch.md))
that row is unblocked, and the first question is what the CTS actually needs that is not here.

## What it needs, counted rather than assumed

Against `opengl-cts-4.6.8.1` (commit `067e8832315e79817ede1c4863804e440f5d1c80`, the peeled tag),
`framework/common` and `framework/opengl` include:

| header | files |
|---|---|
| `<sstream>` | 15 |
| `<ostream>` | 6 |
| `<iostream>` | 4 |
| `<fstream>` | 4 |
| `<iomanip>` | 3 |

and **no** `<thread>`, `<mutex>`, `<regex>` or `<random>` - dEQP does its own threading in C
through `delibs`. So the requirement is **streams**, and streams are localization.

## What we have, counted the same way

`oops-libcxx.mk` builds **two** of libc++'s 45 sources - `verbose_abort.cpp` and `string.cpp` -
and its own comment says why: "the two symbols a string-and-containers link actually needs". That
was right for ACO, which needs eight definitions and no streams.

Compiling all 45 for the target as they stand: **25 compile, 20 fail.** Turning
`_LIBCPP_HAS_LOCALIZATION` on made it *worse* - 18 and 27 - because 21 of the failures became one
missing header, `<xlocale.h>`.

## The decision: a different backend, not a new API

`__locale_dir/locale_base_api.h` selects a locale backend from the predefined macros. We compile
`-target x86_64-unknown-freebsd`, so it takes `support/freebsd.h`, which wants `<xlocale.h>` and
the whole FreeBSD `_l` family - `strtod_l`, `isupper_l`, `mbrtowc_l` and the rest. That family
exists to switch between locales.

**This platform has one locale and will only ever have one.** A console title has no locale
environment to read and no way for anyone to choose one, so a second locale would be a feature
with no source of truth behind it. Writing several dozen `_l` wrappers to express that would be
elaborate agreement with a premise we do not hold.

Upstream already ships the backend for this case. `support/fuchsia.h` takes
`no_locale/characters.h` and `no_locale/strtonum.h`, which are the plain functions with the
locale argument dropped. So:

- `include/__locale_dir/support/freebsd.h` **shadows** upstream's and includes Fuchsia's. The
  pinned tree is not patched; the shadow works because `oops-libcxx.mk` puts this directory ahead
  of the checkout, which is the same mechanism already serving `__config_site` and the
  `mbstate_t` shim.
- `include/locale.h` supplies what Fuchsia's backend still uses from the C library: `locale_t`
  and `newlocale`/`duplocale`/`freelocale`/`uselocale`, over a single static "C" locale, plus a
  real `lconv` because `std::numpunct` reads `localeconv` and a zeroed one formats numbers
  wrongly in silence.
- `include/nl_types.h` declares the message-catalogue API that `__locale_dir/messages.h`
  compiles against. `catopen` always fails and `catgets` returns the caller's own default text,
  which is what the facet is specified to do when there is no catalogue - and there is none,
  because a title ships none.

## Where it stands, and what is left

`<xlocale.h>` is gone. The remaining 28 failures group as:

| cause | files |
|---|---|
| `MB_CUR_MAX` undeclared | **20** |
| threading (`_LIBCPP_HAS_THREADS 0`) | 4 |
| `bad_expected_access`, `aligned_alloc`, `<sys/types.h>`, `shared/fp_bits.h` | 1 each |

**One macro is twenty of them.** `MB_CUR_MAX` is specified to live in `<stdlib.h>` and it is
locale state, so it is the C library's - filed as oops-sdk `REQ-20260923T1810Z-7d42` rather than
worked around here.

It was worked around here first, and that is worth recording because the workaround *looked*
clean: an `include/stdlib.h` doing `#include_next <stdlib.h>` plus the define. It made things
worse - `<cstdlib>`'s own `#include_next` then resolved differently and 24 sources began failing
on `asprintf` that had not been failing before, plus `new.cpp` on `aligned_alloc`. Backed out.
**A header that inserts itself into someone else's `include_next` chain changes every resolution
downstream of it**, which is not visible from the file itself.

Threading is a separate switch and, per the header count above, the CTS framework does not appear
to need it. That is a count of `framework/`, not of the test modules, so it is a reason to leave
`_LIBCPP_HAS_THREADS` alone for now rather than a finding that it can stay off.

## What this does not decide

Nothing about how the CTS is built, packaged or selected from. In particular the subset must be
chosen at **run time** through `--deqp-case`, never by compiling a reduced binary: the worth of a
CTS result is that the tests are not ours, and a suite we pruned at compile time is one we
curated. That argument is already in `oops-mesa`'s roadmap row 8 and this entry does not reopen
it.
