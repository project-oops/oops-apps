/*
 * The freestanding C++ runtime: the handful of symbols a C++ program needs from below the
 * standard library, over `oops-sdk`.
 *
 * **This is shared, and it is small on purpose.** `src/oops-titles/README.md` says the first C++
 * title pays this cost and the rest follow cheaply; this file is that cost, and it is under two
 * hundred lines because everything above it - `std::string`, containers, streams - belongs to the
 * standard library and is somebody else's to maintain, not ours. If this file starts growing
 * type support, that is the signal that a real libc++ is wanted rather than more of this.
 *
 * A title opts in with `USE_CXX = 1`, which also sets `-fno-exceptions -fno-rtti`. Those are not
 * a style preference: they are what the titles queued here actually need. Extreme Tux Racer has
 * **0 `throw` and 0 `dynamic_cast`**, measured. Armagetron has 68 and 167, and is a different
 * project wearing the same name - it needs a runtime this file does not try to be.
 *
 * What is deliberately absent, because the alternative is a lie:
 *
 *   - **Exceptions.** No `__cxa_throw`, no `__gxx_personality_v0`. With `-fno-exceptions` the
 *     compiler never emits a call to either, so a program that needs them fails to *compile*
 *     rather than linking and unwinding into nothing.
 *   - **RTTI.** Same argument, one flag along.
 *   - **Global destructors.** See `__cxa_atexit` below.
 */
#include <stddef.h>
#include <stdint.h>

extern "C" {
#include "oops/heap.h"
}

/* ------------------------------------------------------------------ new/delete
 *
 * Over the SDK's heap, which is where the payload's memory comes from. A failed allocation
 * returns null rather than throwing, because there is nothing to throw - and `-fno-exceptions`
 * means the caller was never going to catch it. That is the `nothrow` contract applied to every
 * form, which C++ permits a freestanding implementation to do and which this states rather than
 * leaves to be discovered.
 *
 * A caller that does not check is a caller that dereferences null and faults *at the point of
 * the bug*, which is the better of the two failures available here.
 */

void *operator new(size_t size)
{
    /* Zero is legal and must return a distinct pointer, so it is rounded up rather than passed
       through to an allocator that may answer null for it. */
    return oops_malloc(size ? size : 1u);
}

void *operator new[](size_t size)
{
    return operator new(size);
}

void operator delete(void *p) noexcept
{
    oops_free(p);
}

/* `aligned_alloc`, which this platform can only half provide, and says so.
 *
 * libc++abi's `__cxa_allocate_exception` reaches `fallback_malloc.cpp`, which reaches libc++'s
 * `__libcpp_aligned_alloc`, which calls `::aligned_alloc`. It is on the road to every `throw`.
 * oops-sdk's freestanding libc has no aligned form, so the C++ runtime supplies one; the
 * declaration is in `src/oops-deps/libcxx/include/stdlib.h`.
 *
 * # Why this is a passthrough and not the usual over-allocation trick
 *
 * The obvious implementation over-allocates, returns the first aligned address inside the block,
 * and stashes the original pointer just below it. **That is wrong here**, and quietly: libc++
 * releases this memory with `__libcpp_aligned_free`, which on every non-MSVC target is a plain
 * `::free(ptr)` on the pointer it was handed. Handing `oops_free` an address `oops_malloc` never
 * returned corrupts the heap at some *other* allocation's expense, and faults somewhere
 * unrelated. So whatever this returns must be free-able directly, which means it must be
 * something `oops_malloc` itself returned.
 *
 * # What the heap actually guarantees, measured rather than assumed
 *
 * `oops_malloc` returns `block + sizeof(heap_block_header_t)`, and that header is
 * `uint32_t, uint32_t, size_t, size_t` - 24 bytes. The blocks underneath are 16 KB page-aligned
 * or 16-byte-rounded slab chunks, so every pointer it hands out is `something 16-aligned + 24`:
 * **8-byte aligned, never 16.**
 *
 * So requests up to 8 are served exactly, and larger ones cannot be served at all without
 * breaking the free contract above. Returning null for those is not a stub: it is the C11
 * answer for a request the allocator cannot meet, and libc++abi handles it by falling back to
 * its own static buffer rather than by crashing.
 *
 * # This is a real limit on exceptions, and it is filed
 *
 * `__cxa_allocate_exception` wants `alignof(__cxa_exception)`, which is 16 on x86-64 - so on
 * this heap it takes the fallback path every time, and that path is a small fixed buffer. The
 * fix is a heap that returns `max_align_t`-aligned pointers, which is oops-sdk's to make and is
 * on the bus. A malloc returning 8-byte-aligned memory is non-conforming for `long double` and
 * for anything SSE-aligned, so this is not a C++-only problem; exceptions are just the first
 * thing to stand on it.
 */
extern "C" void *aligned_alloc(size_t alignment, size_t size)
{
    /* C11 7.22.3.1: the alignment must be a power of two. */
    if (alignment == 0u || (alignment & (alignment - 1u)) != 0u) {
        return nullptr;
    }

    /* What `oops_malloc` guarantees today. See the comment above for how it is derived; if the
       heap's header or block alignment changes, this is the constant that moves with it. */
    const size_t heap_guarantee = 8u;
    if (alignment > heap_guarantee) {
        return nullptr;
    }

    return oops_malloc(size ? size : 1u);
}

void operator delete[](void *p) noexcept
{
    operator delete(p);
}

/* Sized deletes, C++14. The compiler emits these in preference to the unsized forms whenever it
   knows the static type, so leaving them out means half the deletes in a program go to a symbol
   nothing defines - and a payload link would not say so. */
void operator delete(void *p, size_t) noexcept
{
    operator delete(p);
}

void operator delete[](void *p, size_t) noexcept
{
    operator delete(p);
}

/* ==========================================================================================
 * Everything from here to the `__dso_handle` block is **libc++abi's job when libc++abi is
 * linked**, and this file's job when it is not.
 *
 * `OOPS_CXX_EXCEPTIONS` is defined by `common/cxx.mk` when a title opts into exceptions, which
 * is also when it links `libc++abi.a` (oops-apps#D005). libc++abi's `cxa_virtual.cpp` and
 * `cxa_guard.cpp` define these same four symbols, properly, so defining them here as well is a
 * duplicate-symbol link error - which is how this was found, not reasoned about:
 *
 *     ld.lld: error: duplicate symbol: __cxa_guard_acquire
 *     >>> defined at cxxrt.cpp ... and at cxa_guard.cpp
 *
 * **The real one wins.** These are the minimal stand-ins for a library that was not available;
 * once it is, keeping them would mean a title silently using a single-threaded guard
 * implementation while linking a standard library that assumes its own. Stepping aside is the
 * whole reason this file describes itself as small on purpose.
 *
 * What is *not* conditional, deliberately: `operator new`/`delete` above and `__cxa_atexit`
 * below. `oops-libcxxabi.mk` excludes libc++abi's `stdlib_new_delete.cpp` precisely so that
 * allocation keeps going through the SDK heap in both builds, and `__cxa_atexit` is the C
 * library's rather than the ABI library's.
 * ========================================================================================== */
#ifndef OOPS_CXX_EXCEPTIONS

/* ------------------------------------------------------------ pure virtual */

extern "C" void __cxa_pure_virtual(void)
{
    /*
     * A pure virtual reached through a vtable means an object is being used during its own base
     * constructor or after its destructor. There is no recovery and no useful return, so this
     * stops rather than returning into a program whose object graph is already wrong.
     */
    for (;;) {
    }
}

/* ------------------------------------------------- static initialisation guards
 *
 * A function-local static with a non-trivial constructor compiles to: test a guard, and if it is
 * unset, construct and set it. `__cxa_guard_acquire` returns 1 when the caller should construct.
 *
 * **These are thread-safe, and that is why they are written out rather than switched off.**
 * `-fno-threadsafe-statics` would remove the calls and cost nothing in a single-threaded
 * program - but SDL runs the audio callback on its own thread, and a static constructed there
 * for the first time would race the main thread with no diagnostic. Twenty lines of atomics is a
 * smaller price than that bug.
 *
 * The Itanium ABI gives 64 bits per guard and uses the first byte as the "initialised" flag. The
 * byte after it is used here as a lock, which is what a waiter spins on.
 */

extern "C" int __cxa_guard_acquire(uint64_t *guard)
{
    uint8_t *done = reinterpret_cast<uint8_t *>(guard);
    uint8_t *lock = done + 1;

    if (__atomic_load_n(done, __ATOMIC_ACQUIRE) != 0) {
        return 0; /* already constructed */
    }

    for (;;) {
        uint8_t expected = 0;
        if (__atomic_compare_exchange_n(lock, &expected, (uint8_t)1, false,
                                        __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
            break;
        }
        /* Another thread holds the lock. It is either constructing or about to release; when it
           releases, the flag is set, so re-test rather than assume this thread must construct. */
        if (__atomic_load_n(done, __ATOMIC_ACQUIRE) != 0) {
            return 0;
        }
        __builtin_ia32_pause();
    }

    /* Lock held. Re-test: the winner may have finished between the first load and here. */
    if (__atomic_load_n(done, __ATOMIC_ACQUIRE) != 0) {
        __atomic_store_n(lock, (uint8_t)0, __ATOMIC_RELEASE);
        return 0;
    }
    return 1;
}

extern "C" void __cxa_guard_release(uint64_t *guard)
{
    uint8_t *done = reinterpret_cast<uint8_t *>(guard);
    uint8_t *lock = done + 1;

    __atomic_store_n(done, (uint8_t)1, __ATOMIC_RELEASE);
    __atomic_store_n(lock, (uint8_t)0, __ATOMIC_RELEASE);
}

extern "C" void __cxa_guard_abort(uint64_t *guard)
{
    /* Construction failed, so the flag stays clear and the next caller tries again. Only the
       lock is dropped. Reached only from a throwing constructor, which `-fno-exceptions` rules
       out - it is here because the ABI names it and a missing symbol would link silently. */
    uint8_t *lock = reinterpret_cast<uint8_t *>(guard) + 1;
    __atomic_store_n(lock, (uint8_t)0, __ATOMIC_RELEASE);
}

#endif /* !OOPS_CXX_EXCEPTIONS - libc++abi provides the four symbols above */

/* --------------------------------------------------------------- destructors */

extern "C" {

/* The ABI requires the address of this symbol; nothing reads its contents. */
void *__dso_handle = &__dso_handle;

/*
 * **Global destructors do not run, and this is not an oversight.**
 *
 * `__cxa_atexit` registers a destructor to be called when the program exits normally. A payload
 * has no normal exit: `oops-sdk`'s own `exit` goes straight to the platform's `SYS_exit` and says
 * in its comment that there is no `main` to return through and no atexit list to run. Registering
 * destructors that will never be called, in a list that costs memory to keep, would be
 * bookkeeping for an event that cannot happen.
 *
 * Returning 0 says the registration succeeded, which is what the caller needs to hear to carry
 * on - and the honest alternative, failing, would abort a program at start-up over a cleanup
 * path it will never reach. A title that must release something at the end does it before
 * calling `exit`, as it would on any system.
 */
int __cxa_atexit(void (*destructor)(void *), void *arg, void *dso)
{
    (void)destructor;
    (void)arg;
    (void)dso;
    return 0;
}

} /* extern "C" */
