/*
 * cxx-throw - does a C++ exception survive a round trip on this platform?
 *
 * The platform exports none of the Itanium unwind or C++ ABI surface: obSCEne swept all
 * 19 symbols across `libkernel`, `libSceLibcInternal` and `self` and found every one
 * absent at `0x0` (REQ-20260921T0953Z-e3f7). It runs an MSVC-lineage SEH unwinder
 * instead. So a title that throws links its own libunwind and libc++abi, and this probe
 * is the thing that says whether that actually works. (oops-apps#D005)
 *
 * The checks are deliberately ordinary C++ and deliberately small. A probe that fails
 * should point at the unwinder, not at itself.
 */
#ifndef CXX_THROW_H
#define CXX_THROW_H

/* A plain type, thrown and caught by value-reference. The simplest case that still
   needs the full machinery: allocate, throw, unwind, match the type, catch. */
struct BoundaryError {
    int code;
};

/* A derived type, to check that `catch (const BoundaryError &)` matches it. That match
   is done at run time by libc++abi's `private_typeinfo.cpp` walking the type hierarchy,
   so it exercises a different part of the ABI than the exact-type case - and it is the
   part that needs RTTI, which is why `OOPS_CXX_EXCEPTIONS` turns on `-frtti` as well as
   `-fexceptions`. */
struct DerivedError : BoundaryError {
    explicit DerivedError(int c) : BoundaryError{c} {}
};

/* Both defined in `boundary.cpp`, a different translation unit on purpose - see the
   comment there for why that is the whole point. */
void cxx_throw_raise(int code);
void cxx_throw_raise_derived(int code);

/* The probe itself, in `probe.cpp`. Returns the number of checks that passed; the total
   is `cxx_throw_total()`. Written as a count-and-compare rather than a bool so a
   partial result is distinguishable from a total failure, which is the difference
   between "the unwinder is wrong" and "the unwinder is absent". */
extern "C" int cxx_throw_run(void);
extern "C" int cxx_throw_total(void);

#endif /* CXX_THROW_H */
