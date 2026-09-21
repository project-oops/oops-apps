/*
 * The far side of the boundary. This throws and never catches, so the exception has to be
 * carried back out by the unwinder rather than handled where it was raised.
 *
 * **Why a separate translation unit matters.** Within one object, clang can sometimes see the
 * throw and the catch together and reduce the whole thing to a branch, which would prove
 * nothing about `_Unwind_RaiseException`. Across a translation unit the compiler must emit a
 * real `__cxa_throw` and real unwind tables, and the linker must have placed those tables where
 * `__eh_frame_start` says they are. That is the property the probe is here to test.
 *
 * `visibility("default")` because a title is linked `-shared` with `-Bsymbolic`: the call has to
 * survive as a real cross-object call rather than being hidden and inlined away.
 */
#include "cxx_throw.h"

__attribute__((visibility("default"))) void cxx_throw_raise(int code)
{
    throw BoundaryError{code};
}

__attribute__((visibility("default"))) void cxx_throw_raise_derived(int code)
{
    throw DerivedError{code};
}
