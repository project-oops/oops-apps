/*
 * The checks. Each is a throw that must come back as a catch, and each compares the
 * value carried by the exception object, so a catch with a corrupted payload fails
 * differently from a catch that does not run.
 */
#include "cxx_throw.h"

namespace {

/* 1. The basic round trip: throw across the boundary, catch by const reference, and
   check the payload arrived intact. */
bool check_basic() {
    try {
        cxx_throw_raise(0x5151);
    } catch (const BoundaryError &e) {
        return e.code == 0x5151;
    } catch (...) {
        return false;
    }
    return false; /* Reaching here means the throw did not happen at all. */
}

/* 2. A derived type caught as its base: libc++abi's run-time type walk, which needs
   RTTI. */
bool check_derived_matches_base() {
    try {
        cxx_throw_raise_derived(0x6262);
    } catch (const BoundaryError &e) {
        return e.code == 0x6262;
    } catch (...) {
        return false;
    }
    return false;
}

/* 3. Destructors run as the stack unwinds: the personality routine finds and runs
   cleanup landing pads, a separate mechanism from delivering the exception. */
int g_unwound = 0;
struct MarksItsOwnUnwinding {
    ~MarksItsOwnUnwinding() { g_unwound += 1; }
};

bool check_destructors_run_while_unwinding() {
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

/* 4. Rethrow. `throw;` goes through `__cxa_rethrow`, which keeps the original exception
   object alive across a second unwind. */
bool check_rethrow() {
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

/* 5. Two plain C++ frames between throw and catch, so the unwinder walks more than one
   frame. */
[[gnu::noinline]] void middle_frame_two() {
    cxx_throw_raise(0x9595);
}
[[gnu::noinline]] void middle_frame_one() {
    middle_frame_two();
}

bool check_multi_frame() {
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
    check_basic,   check_derived_matches_base, check_destructors_run_while_unwinding,
    check_rethrow, check_multi_frame,
};

const char *const kCheckNames[] = {
    "basic", "derived-matches-base", "destructors-run", "rethrow", "multi-frame",
};

} /* namespace */

/* Each check's name is logged before it runs, so the last name printed is the check
   that did not come back. */
extern "C" void oops_klog(const char *tag, const char *msg);

namespace {

/* Installed from `cxx_throw_run` rather than a static constructor: nothing walks
   `.init_array` in this title, and the probe does not depend on a mechanism it does not
   test. */
void terminate_says_so() {
    oops_klog("cxx-throw",
              "std::terminate - the check named above threw and no handler ran");
    __builtin_trap();
}

} /* namespace */

namespace std {
using terminate_handler = void (*)();
terminate_handler set_terminate(terminate_handler) noexcept;
} /* namespace std */

extern "C" int cxx_throw_total(void) {
    return static_cast<int>(sizeof(kChecks) / sizeof(kChecks[0]));
}

extern "C" int cxx_throw_run(void) {
    std::set_terminate(terminate_says_so);

    int passed = 0;
    for (int i = 0; i < cxx_throw_total(); ++i) {
        oops_klog("cxx-throw", kCheckNames[i]);
        if (kChecks[i]()) {
            passed += 1;
            oops_klog("cxx-throw", "  ^ came back through the unwinder");
        } else {
            oops_klog("cxx-throw",
                      "  ^ returned, but the wrong way - handler ran with bad state");
        }
    }
    return passed;
}
