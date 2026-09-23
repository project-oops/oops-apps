/*
 * The checks. Each one is a throw that must come back as a catch, and each is written so that a
 * failure is distinguishable from the next one along rather than collapsing into "it did not
 * work".
 *
 * **Nothing here counts a success it did not observe.** CONVENTIONS section 3: assert on the
 * failure, never on the count of passes. So every check compares a value that could only have
 * come through the unwinder - the payload carried by the exception object - rather than merely
 * recording that the catch block was entered. A catch that runs with a corrupted payload is a
 * different bug from a catch that does not run, and a probe that cannot tell them apart is not
 * worth running on hardware.
 */
#include "cxx_throw.h"

namespace {

/* 1. The basic round trip: throw across the boundary, catch by const reference, and check the
      payload arrived intact. */
bool check_basic()
{
    try {
        cxx_throw_raise(0x5151);
    } catch (const BoundaryError &e) {
        return e.code == 0x5151;
    } catch (...) {
        return false;
    }
    return false; /* Reaching here means the throw did not happen at all. */
}

/* 2. A derived type caught as its base. This is the run-time type walk in libc++abi, and it is
      the check that needs RTTI rather than just unwinding. */
bool check_derived_matches_base()
{
    try {
        cxx_throw_raise_derived(0x6262);
    } catch (const BoundaryError &e) {
        return e.code == 0x6262;
    } catch (...) {
        return false;
    }
    return false;
}

/* 3. Destructors must run as the stack is unwound. This is the property that makes exceptions
      usable rather than merely survivable, and it is a separate mechanism from delivering the
      exception: the personality routine has to find and run cleanup landing pads on the way
      past. A platform where the catch works and this does not would leak every RAII object on
      every throw. */
int g_unwound = 0;
struct MarksItsOwnUnwinding {
    ~MarksItsOwnUnwinding() { g_unwound += 1; }
};

bool check_destructors_run_while_unwinding()
{
    g_unwound = 0;
    try {
        MarksItsOwnUnwinding a;
        MarksItsOwnUnwinding b;
        cxx_throw_raise(0x7373);
    } catch (const BoundaryError &) {
        /* Both locals were live at the throw, so both destructors owe a call. */
        return g_unwound == 2;
    } catch (...) {
        return false;
    }
    return false;
}

/* 4. Rethrow. `throw;` inside a handler goes back through `__cxa_rethrow`, which is a different
      entry point from `__cxa_throw` and keeps the original exception object alive across a
      second unwind. It is the one that catches a reference-counting mistake. */
bool check_rethrow()
{
    try {
        try {
            cxx_throw_raise(0x8484);
        } catch (const BoundaryError &) {
            throw;
        }
    } catch (const BoundaryError &e) {
        return e.code == 0x8484;
    } catch (...) {
        return false;
    }
    return false;
}

/* 5. An exception thrown and caught through two frames of plain C++ in between, so the unwinder
      has to walk more than one frame rather than finding the handler immediately above. */
[[gnu::noinline]] void middle_frame_two() { cxx_throw_raise(0x9595); }
[[gnu::noinline]] void middle_frame_one() { middle_frame_two(); }

bool check_multi_frame()
{
    try {
        middle_frame_one();
    } catch (const BoundaryError &e) {
        return e.code == 0x9595;
    } catch (...) {
        return false;
    }
    return false;
}

bool (*const kChecks[])() = {
    check_basic,
    check_derived_matches_base,
    check_destructors_run_while_unwinding,
    check_rethrow,
    check_multi_frame,
};

const char *const kCheckNames[] = {
    "basic", "derived-matches-base", "destructors-run", "rethrow", "multi-frame",
};

} /* namespace */

/*
 * Saying which check, and saying it before the check runs.
 *
 * The first two hardware runs printed `running exception checks` and then a libc++abi terminate
 * message naming only the exception *type*, which three of the five checks throw. So the report
 * could not say which one died, and the difference matters - `basic` failing is an unwinder that
 * does not work at all, `destructors-run` failing is one that works and skips cleanups.
 *
 * A line before each check is the whole fix: the last name printed is the check that did not
 * come back, because a check that terminates never reaches its own result line.
 */
extern "C" void oops_klog(const char *tag, const char *msg);

namespace {

/*
 * The terminate handler is installed here rather than by a static constructor.
 *
 * `common/cxxrt.cpp` installs one from a namespace-scope object, and on hardware it never ran -
 * libc++abi's own default handler printed the message instead. **Nothing walks `.init_array` in
 * this title.** oops-mesa titles call `oops_mesa_run_init_array` for exactly this reason and a
 * plain C++ title has no equivalent, so a static constructor here is silently dead code.
 *
 * Calling `set_terminate` from `cxx_throw_run` is not a workaround for that; it is the honest
 * placement for a probe, which should not depend on a mechanism it is not testing.
 */
void terminate_says_so()
{
    oops_klog("cxx-throw", "std::terminate - the check named above threw and no handler ran");
    __builtin_trap();
}

} /* namespace */

namespace std {
using terminate_handler = void (*)();
terminate_handler set_terminate(terminate_handler) noexcept;
} /* namespace std */

extern "C" int cxx_throw_total(void)
{
    return static_cast<int>(sizeof(kChecks) / sizeof(kChecks[0]));
}

extern "C" int cxx_throw_run(void)
{
    std::set_terminate(terminate_says_so);

    int passed = 0;
    for (int i = 0; i < cxx_throw_total(); ++i) {
        oops_klog("cxx-throw", kCheckNames[i]);
        if (kChecks[i]()) {
            passed += 1;
            oops_klog("cxx-throw", "  ^ came back through the unwinder");
        } else {
            oops_klog("cxx-throw", "  ^ returned, but the wrong way - handler ran with bad state");
        }
    }
    return passed;
}
