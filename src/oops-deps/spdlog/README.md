# spdlog

Pinned at `v1.16.0`, the revision `libultraship` asks for. `#include <spdlog/spdlog.h>` compiles
against this collection's freestanding libc++. Ship of Harkinian's tree includes a spdlog header
more often than any of its other dependencies.

## What it needs

| Need | Answer | Where |
|---|---|---|
| `std::thread`, `std::mutex`, `std::recursive_mutex`, `std::condition_variable` | libc++'s external threading API, mapped onto the SDK | `../libcxx/include/__external_threading`, `_LIBCPP_HAS_THREADS 1` |
| recursive mutex, TLS | SDK entry points | `oops_mutex_init_recursive`, `oops_tls_*` |
| monotonic clock (required by threads) | `clock_gettime` over `oops_time_get_ns` | `common/posix/posix.c` |
| `fcntl.h`, `pthread_np.h` | shim headers | `common/posix/include/` |
| `getpid`, `fileno`, `fstat`, `isatty`, `fsync` | shim functions | `common/posix/posix.c` |
| `std::wstring` | `patches/0001` | one guarded method with no caller |
| `std::numpunct<wchar_t>` | `-DFMT_USE_LOCALE=0` | upstream's own switch - a flag, not a patch |

Everything except the patch is shared: another threaded C++ port gets `std::mutex` and
`std::thread` from the same libc++ configuration, and the shim functions are ordinary POSIX.

`oops-spdlog.mk` is header-only.
