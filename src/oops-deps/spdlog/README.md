# spdlog

Pinned at `v1.16.0`, the revision `libultraship` asks for. Fetched; **not yet building**, and
what stops it is not spdlog.

## Why this one first

It is the largest of Ship of Harkinian's nine dependencies and the last one anyone would choose
by size. It is first because the code chooses it: **123** of that tree's files include a spdlog
header, against 42 for `nlohmann/json` and 40 for `tinyxml2`. Nothing else there can be compiled
and checked until this is in.

## What it actually needs

Compiled `#include <spdlog/spdlog.h>` against this collection's freestanding libc++ on
2026-09-25, with the flag set a working C++ title (`extreme-tux-racer`) really uses. Fifteen
errors, in three groups, and all three are the standard library rather than spdlog:

| gap | errors | where |
|---|---|---|
| **`std::thread`, `std::mutex`, `std::recursive_mutex`, `std::condition_variable`** | 11 | `details/periodic_worker.h`, `details/registry.h` |
| `std::wstring` | 2 | `fmt/bundled/format.h` |
| `fcntl.h` not found | 1 (fatal) | `details/os-inl.h` |

`src/oops-deps/libcxx/include/__config_site` says why:

```
#define _LIBCPP_HAS_THREADS          0
#define _LIBCPP_HAS_WIDE_CHARACTERS  0
```

**The threading half is the real work, and it is not a missing feature so much as an unwired
one.** `oops-sdk` already has the whole API — `oops_thread_create`, `oops_thread_join`,
`oops_thread_detach`, `oops_thread_self`, `oops_mutex_*`, `oops_sem_*`. libc++ has a supported
mechanism for exactly this situation, `_LIBCPP_HAS_THREAD_API_EXTERNAL`, which takes a small
`__external_threading` header mapping its primitives onto whatever the platform has. That is the
shape of the job.

So this is an `oops-deps/libcxx` task before it is a spdlog task, and it is worth doing on those
terms: every threaded C++ port after this one needs the same thing, and `std::mutex` is not an
exotic requirement.

The other two are smaller. Wide characters are a `__config_site` switch plus whatever `<cwchar>`
support the libc side needs; `fcntl.h` is a header the POSIX shim in `common/posix/` does not yet
provide, and spdlog reaches it only for file sinks, which a console port may not want at all.

## Not vendored yet

There is no `oops-spdlog.mk` here on purpose. A build file for something that cannot compile
would be scaffolding, and the numbers above are more useful than a rule that fails.
