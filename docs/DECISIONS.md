# Decisions

The decisions in force, one file each under `decisions/`. Format and rules are in
[STYLE](https://github.com/project-oops/OOPS/blob/main/docs/STYLE.md#decisions).

**This table is generated.** Edit an entry under `decisions/`, then run
`tools/split-decisions.sh --index oops-apps`. A number resolves to exactly one file.

| | # | decision | status | date |
|---|---|---|---|---|
| 🟢 | D001 | [One repository for apps built on the SDK](decisions/D001-one-repository-for-apps-built-on-the-sdk.md) | decided | 2026-09-26 |
| 🟢 | D003 | [Porthole builds without the encoder session](decisions/D003-porthole-builds-without-the-encoder-session.md) | decided | 2026-09-26 |
| 🟢 | D004 | [Porthole captures and serves; it does not encode or apply input](decisions/D004-porthole-captures-and-serves.md) | decided | 2026-09-26 |
| 🟢 | D005 | [Exceptions come from our own libunwind](decisions/D005-exceptions-come-from-our-own-libunwind.md) | decided | 2026-09-26 |
| 🟢 | D006 | [The frame table is kept, readable, and covers the unwinder](decisions/D006-the-frame-table-is-kept-readable-and-covers-the-unwinder.md) | decided | 2026-09-26 |
| 🟢 | D007 | [libc++ uses the no-locale backend](decisions/D007-libcxx-uses-the-no-locale-backend.md) | decided | 2026-09-26 |

| | meaning |
|---|---|
| 🟢 | settled, and the reasoning rests on something checkable |
| 🟡 | assumed or proposed - made without input, and in the review queue |
| 🔴 | reversed, superseded or blocked |
| ⚪ | no status recorded |

A date with `~` is **not recorded** - it is worked out from the dated entries either
side, because an entry between two of them was written between their dates. `~` alone
is a day both neighbours agree on; `~a..b` is a span, and no day inside it is claimed;
`~>a` and `~<a` are entries with a dated neighbour on only one side. A bare `-` has no
dated entry either side to reason from.
