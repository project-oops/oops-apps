/*
 * The freestanding C++ runtime: the symbols a C++ program needs from below the standard
 * library, over `oops-sdk`. Type support belongs to a standard library, not here.
 *
 * Built by `common/cxx.mk`. Without OOPS_CXX_EXCEPTIONS a title compiles with
 * `-fno-exceptions -fno-rtti`, so no `__cxa_throw` or personality routine is needed;
 * with it, libc++abi supplies them and the guard and pure-virtual symbols below.
 * Global destructors do not run (see `__cxa_atexit`).
 */
#include <stddef.h>
#include <stdint.h>

extern "C" {
#include "oops/heap.h"
}

/* new/delete over the SDK heap. A failed allocation returns null: the `nothrow`
 * contract for every form, which a freestanding implementation may use.
 *
 * A title linking the full `libc++.a` sets OOPS_CXX_EXTERNAL_NEW_DELETE. libc++'s
 * forms live in `__lcxx_override` and trap (`ud2`) if any replaceable form resolves
 * outside it; its allocations still reach the SDK heap through `malloc`
 * (oops-sdk src/system/libc.c).
 */
#ifndef OOPS_CXX_EXTERNAL_NEW_DELETE

void *operator new(size_t size) {
    /* Zero is legal and must return a distinct pointer, so it is rounded up rather than
       passed through to an allocator that may answer null for it. */
    return oops_malloc(size ? size : 1u);
}

void *operator new[](size_t size) {
    return operator new(size);
}

void operator delete(void *p) noexcept {
    oops_free(p);
}

/* `aligned_alloc` is oops-sdk's (src/memory/heap.c): libc++ frees aligned blocks with
 * plain `free`, so the aligned pointer must carry its own block header. */

void operator delete[](void *p) noexcept {
    operator delete(p);
}

/* Sized deletes, C++14. The compiler emits these whenever it knows the static type, and
   a payload link does not report a missing symbol. */
void operator delete(void *p, size_t) noexcept {
    operator delete(p);
}

void operator delete[](void *p, size_t) noexcept {
    operator delete(p);
}

#endif /* OOPS_CXX_EXTERNAL_NEW_DELETE */

/* Up to `__dso_handle`: stand-ins for symbols libc++abi defines (`cxa_virtual.cpp`,
 * `cxa_guard.cpp`) when a title links it with OOPS_CXX_EXCEPTIONS (oops-apps#D005).
 * `operator new`/`delete` and `__cxa_atexit` stay unconditional; `oops-libcxxabi.mk`
 * excludes libc++abi's `stdlib_new_delete.cpp`.
 */
#ifndef OOPS_CXX_EXCEPTIONS

/* ------------------------------------------------------------ pure virtual */

extern "C" void __cxa_pure_virtual(void) {
    /*
     * A pure virtual reached through a vtable means an object is being used during its
     * own base constructor or after its destructor. There is no recovery and no useful
     * return, so this stops rather than returning into a program whose object graph is
     * already wrong.
     */
    for (;;) {
    }
}

/* ------------------------------------------------- static initialisation guards
 *
 * `__cxa_guard_acquire` returns 1 when the caller should construct the static. The
 * guards are thread-safe because SDL runs the audio callback on its own thread. The
 * Itanium ABI uses the guard's first byte as the "initialised" flag; the second byte
 * is a spin lock.
 */

extern "C" int __cxa_guard_acquire(uint64_t *guard) {
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
        /* Another thread holds the lock. It is either constructing or about to release;
           when it releases, the flag is set, so re-test rather than assume this thread
           must construct. */
        if (__atomic_load_n(done, __ATOMIC_ACQUIRE) != 0) {
            return 0;
        }
        __builtin_ia32_pause();
    }

    /* Lock held. Re-test: the winner may have finished between the first load and here.
     */
    if (__atomic_load_n(done, __ATOMIC_ACQUIRE) != 0) {
        __atomic_store_n(lock, (uint8_t)0, __ATOMIC_RELEASE);
        return 0;
    }
    return 1;
}

extern "C" void __cxa_guard_release(uint64_t *guard) {
    uint8_t *done = reinterpret_cast<uint8_t *>(guard);
    uint8_t *lock = done + 1;

    __atomic_store_n(done, (uint8_t)1, __ATOMIC_RELEASE);
    __atomic_store_n(lock, (uint8_t)0, __ATOMIC_RELEASE);
}

extern "C" void __cxa_guard_abort(uint64_t *guard) {
    /* Construction threw: the flag stays clear and only the lock is dropped, so the
       next caller tries again. */
    uint8_t *lock = reinterpret_cast<uint8_t *>(guard) + 1;
    __atomic_store_n(lock, (uint8_t)0, __ATOMIC_RELEASE);
}

#endif /* !OOPS_CXX_EXCEPTIONS - libc++abi provides the four symbols above */

/* --------------------------------------------------------------- destructors */

extern "C" {

/*
 * The ABI requires the address of this symbol; nothing reads its contents. Weak
 * because oops-mesa's `src/runtime/abi.c` defines it too, and a C++ Mesa title links
 * both. There is one shared object, so either definition means the same thing.
 */
__attribute__((weak)) void *__dso_handle = &__dso_handle;

/*
 * Global destructors do not run: a payload has no normal exit, and oops-sdk's `exit`
 * has no atexit list. Registration reports success so start-up continues; a title
 * releases what it must before calling `exit`.
 */
int __cxa_atexit(void (*destructor)(void *), void *arg, void *dso) {
    (void)destructor;
    (void)arg;
    (void)dso;
    return 0;
}

} /* extern "C" */

#ifdef OOPS_CXX_EXCEPTIONS

/*
 * Declared rather than included: `<exception>` pulls in `<cstdlib>`, which needs
 * libc++'s include directory ahead of the C library's. `set_terminate` is in namespace
 * `std`, where libc++abi defines it (`cxa_handlers.cpp`), so the mangled names match.
 */
namespace std {
using terminate_handler = void (*)();
terminate_handler set_terminate(terminate_handler) noexcept;
} /* namespace std */

extern "C" void oops_klog(const char *tag, const char *msg);
extern "C" void abort(void) __attribute__((noreturn));

/*
 * A terminate handler that logs, installed by a static constructor before `main`.
 * libc++abi's default handler reports through `__abort_message`, which is hidden and
 * prints nothing here. The log line separates a throw that found no handler from a
 * fault unrelated to exceptions. `.init_array` runs in both Mesa and hosted titles.
 */
namespace {

void oops_cxx_terminate_handler() {
    oops_klog(
        "cxx",
        "std::terminate reached - an exception found no handler, or a handler threw");
    /* The standard requires terminate to end the program; `abort` logs and parks. */
    abort();
}

struct oops_cxx_terminate_installer {
    oops_cxx_terminate_installer() { std::set_terminate(oops_cxx_terminate_handler); }
};

const oops_cxx_terminate_installer s_install_terminate;

} /* namespace */
#endif /* OOPS_CXX_EXCEPTIONS */

/* ------------------------------------------------------ complex multiplication */

/*
 * `__muldc3` and `__mulsc3`, which clang emits for complex multiplication and which
 * compiler-rt normally supplies; a payload links `-nostdlib`. The algorithm is C99
 * Annex G.5.1: when both parts come out NaN, recover an infinity from infinite
 * operands. Real arithmetic and a bitwise sign copy keep either from lowering into a
 * call to itself.
 */
namespace {

template <typename T> struct oops_cx_bits;
template <> struct oops_cx_bits<double> {
    typedef uint64_t type;
    static const int shift = 63;
};
template <> struct oops_cx_bits<float> {
    typedef uint32_t type;
    static const int shift = 31;
};

template <typename T> bool oops_cx_isnan(T x) {
    return x != x;
}
template <typename T> bool oops_cx_isinf(T x) {
    return x == x && (x - x) != (x - x);
}

/* `magnitude` with the sign of `sign`, on the representation. */
template <typename T> T oops_cx_copysign(T magnitude, T sign) {
    typedef typename oops_cx_bits<T>::type U;
    const U mask = (U)1 << oops_cx_bits<T>::shift;
    U m, s;
    __builtin_memcpy(&m, &magnitude, sizeof m);
    __builtin_memcpy(&s, &sign, sizeof s);
    m = (m & ~mask) | (s & mask);
    T out;
    __builtin_memcpy(&out, &m, sizeof out);
    return out;
}

template <typename T> void oops_cx_mul(T a, T b, T c, T d, T *re, T *im) {
    const T ac = a * c, bd = b * d, ad = a * d, bc = b * c;
    T x = ac - bd;
    T y = ad + bc;
    if (oops_cx_isnan(x) && oops_cx_isnan(y)) {
        bool recalc = false;
        if (oops_cx_isinf(a) || oops_cx_isinf(b)) {
            a = oops_cx_copysign(oops_cx_isinf(a) ? (T)1 : (T)0, a);
            b = oops_cx_copysign(oops_cx_isinf(b) ? (T)1 : (T)0, b);
            if (oops_cx_isnan(c))
                c = oops_cx_copysign((T)0, c);
            if (oops_cx_isnan(d))
                d = oops_cx_copysign((T)0, d);
            recalc = true;
        }
        if (oops_cx_isinf(c) || oops_cx_isinf(d)) {
            c = oops_cx_copysign(oops_cx_isinf(c) ? (T)1 : (T)0, c);
            d = oops_cx_copysign(oops_cx_isinf(d) ? (T)1 : (T)0, d);
            if (oops_cx_isnan(a))
                a = oops_cx_copysign((T)0, a);
            if (oops_cx_isnan(b))
                b = oops_cx_copysign((T)0, b);
            recalc = true;
        }
        if (!recalc && (oops_cx_isinf(ac) || oops_cx_isinf(bd) || oops_cx_isinf(ad) ||
                        oops_cx_isinf(bc))) {
            if (oops_cx_isnan(a))
                a = oops_cx_copysign((T)0, a);
            if (oops_cx_isnan(b))
                b = oops_cx_copysign((T)0, b);
            if (oops_cx_isnan(c))
                c = oops_cx_copysign((T)0, c);
            if (oops_cx_isnan(d))
                d = oops_cx_copysign((T)0, d);
            recalc = true;
        }
        if (recalc) {
            const T inf = __builtin_inf();
            x = inf * (a * c - b * d);
            y = inf * (a * d + b * c);
        }
    }
    *re = x;
    *im = y;
}

} /* namespace */

extern "C" double _Complex __muldc3(double a, double b, double c, double d) {
    double re, im;
    oops_cx_mul(a, b, c, d, &re, &im);
    double _Complex z;
    __real__ z = re;
    __imag__ z = im;
    return z;
}

extern "C" float _Complex __mulsc3(float a, float b, float c, float d) {
    float re, im;
    oops_cx_mul(a, b, c, d, &re, &im);
    float _Complex z;
    __real__ z = re;
    __imag__ z = im;
    return z;
}
