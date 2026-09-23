# Decisions

Numbered, with reasoning, as they are made. The reasoning is the point - it is what stops a
choice being re-litigated by somebody who only has the choice.

**This table is generated.** Edit an entry under `decisions/`, then run
`tools/split-decisions.sh --index oops-apps`. A number resolves to exactly one file.

| | # | decision | status | date |
|---|---|---|---|---|
| 🟢 | D001 | [A repository for apps built on the SDK, and obSCEne is not one](decisions/D001-a-repository-for-apps-built-on-the-sdk.md) | decided | 2026-09-04 |
| 🟢 | D002 | [The base layer is on loan, and belongs in the SDK](decisions/D002-the-base-layer-is-on-loan-from-obscene.md) | decided | 2026-09-04 |
| 🟢 | D003 | [The encoder session is gated off until its parameter layouts are known](decisions/D003-the-encoder-session-is-gated-off-until.md) | decided | 2026-09-07 |
| 🟢 | D004 | [Hardware encode and pad injection are both out of reach from a payload](decisions/D004-hardware-encode-and-pad-injection-are.md) | hardware | 2026-09-09 |
| 🟢 | D005 | [Exceptions come from our own libunwind, and one measurement gates the build shape](decisions/D005-exceptions-come-from-our-own-libunwind-and-one-measurement-gates-it.md) | decided | 2026-09-21 |
| 🟢 | D006 | [Three things had to be true for a `throw` to reach its `catch`, and each one hid the next](decisions/D006-three-things-had-to-be-true-for-a-throw-to-reach-its-catch.md) | decided | 2026-09-23 |

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
