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

## Where it ended: localization is on and streams link

**`_LIBCPP_HAS_LOCALIZATION` is 1, libc++ builds 33 sources where it built 2, and a translation
unit using `std::ostringstream`, `std::istringstream`, `std::string`, `std::runtime_error` and a
`throw`/`catch` links against `libc++.a` + `libc++abi.a` + `libunwind.a` with zero unresolved
symbols.** That is the CTS framework's exact shape.

Working back from `<xlocale.h>`, each cause in series and what answered it:

| cause | files it blocked | answer | whose |
|---|---|---|---|
| `<xlocale.h>` | 21 | select the no-locale backend | here |
| `MB_CUR_MAX` | 20 | `<stdlib.h>`, value fixed at 1 | oops-sdk |
| `asprintf` | 23 | implemented over `vsnprintf`, growing | oops-sdk |
| `strcoll` / `strxfrm` | 23 | `strcmp` and a bounded copy, which is what the C locale specifies | oops-sdk |
| `_CTYPE_*` masks | 22 | FreeBSD's values, cited | oops-sdk |
| `struct tm` undeclared before `<cwchar>` | 21 | a `<wchar.h>` that declares types and no functions | oops-sdk |
| `ungetc` | 3 (every stream) | one character of pushback, in `FILE` | oops-sdk |
| `_DefaultRuneLocale` | 1 (`locale.cpp`) | `runetype.h` + a generated C-locale table | here |

`MB_CUR_MAX` and the rest went to oops-sdk as `REQ-20260923T1810Z-7d42` and were then done
directly rather than waited on. They belong there on the merits: every one is a C library
facility that the C standard or POSIX puts in a named header, and none of them knows what libc++
is.

The two that stayed here are the two that are *about* libc++: choosing its locale backend, and
`_DefaultRuneLocale`, which is FreeBSD libc's own symbol that this SDK is not obliged to have.
oops-sdk's `<ctype.h>` answers each class with an inline comparison and has no rune concept;
giving it one for a consumer that is not the C library would be shaping the SDK around libc++.
The table is generated from those same inline predicates by `tools/gen-rune-table.py`, so
`isalpha(c)` and `classic_table()[c] & _CTYPE_A` cannot disagree - the one failure mode a
hand-written 256-entry table would invite and nothing would catch.

### What is still off, and deliberately

13 of the 45 do not compile, and the list is no longer a blocker but a description:

| cause | files |
|---|---|
| threading (`_LIBCPP_HAS_THREADS 0`) | 9 |
| `timeval`, `random_device`, `bad_expected_access`, `shared/fp_bits.h` | 1 each |

Threading is the big one and the CTS framework does not appear to need it - `framework/`
includes no `<thread>` or `<mutex>`, because dEQP threads itself in C through `delibs`. That is a
count of the framework, not of the test modules, so it is a reason not to turn threads on *yet*
rather than a finding that they can stay off.

It was worked around here first, as an `include/stdlib.h` doing `#include_next <stdlib.h>` plus
the define, and the failure count went *up* - 24 sources started failing on `asprintf` that had
not been failing before. That was read as the shadow breaking `<cstdlib>`'s own `#include_next`
chain, and it was **wrong**: when oops-sdk supplied `MB_CUR_MAX` properly, the same 24 failures
appeared. The shadow had not caused them, it had *revealed* them, by getting 20 files past the
macro so they could reach the next missing name.

The real lesson is about reading the number. **A failure count is not a progress bar when the
causes are in series**: fixing the blocker that 20 files share moves all 20 onto whatever is
behind it, and the total can rise while the work is going well. Only the *cause* histogram means
anything, which is why the table below groups by cause and not by file.

Threading is a separate switch and, per the header count above, the CTS framework does not appear
to need it. That is a count of `framework/`, not of the test modules, so it is a reason to leave
`_LIBCPP_HAS_THREADS` alone for now rather than a finding that it can stay off.

## What this does not decide

Nothing about how the CTS is built, packaged or selected from. In particular the subset must be
chosen at **run time** through `--deqp-case`, never by compiling a reduced binary: the worth of a
CTS result is that the tests are not ours, and a suite we pruned at compile time is one we
curated. That argument is already in `oops-mesa`'s roadmap row 8 and this entry does not reopen
it.
