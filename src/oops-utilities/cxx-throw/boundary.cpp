/*
 * The far side of the boundary: throws and never catches, so the unwinder carries the
 * exception out. A separate translation unit, so the compiler emits a real
 * `__cxa_throw` and unwind tables instead of reducing throw and catch to a branch.
 * `visibility("default")` keeps the call a real call in a title linked
 * `-shared -Bsymbolic`.
 */
#include "cxx_throw.h"

__attribute__((visibility("default"))) void cxx_throw_raise(int code) {
    throw BoundaryError{code};
}

__attribute__((visibility("default"))) void cxx_throw_raise_derived(int code) {
    throw DerivedError{code};
}
