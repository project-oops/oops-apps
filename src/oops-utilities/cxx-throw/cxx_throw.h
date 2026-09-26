/*
 * cxx-throw - does a C++ exception survive a round trip on this platform?
 *
 * The platform exports no Itanium unwind or C++ ABI symbols, so a title that throws
 * links its own libunwind and libc++abi (oops-apps#D005). The checks are small,
 * ordinary C++, so a failure points at the unwinder.
 */
#ifndef CXX_THROW_H
#define CXX_THROW_H

/* A plain type: allocate, throw, unwind, match the type, catch. */
struct BoundaryError {
    int code;
};

/* A derived type caught as `const BoundaryError &`: the run-time type walk in
   libc++abi's `private_typeinfo.cpp`, which needs RTTI. */
struct DerivedError : BoundaryError {
    explicit DerivedError(int c) : BoundaryError{c} {}
};

/* Both defined in `boundary.cpp`, a separate translation unit. */
void cxx_throw_raise(int code);
void cxx_throw_raise_derived(int code);

/* The probe, in `probe.cpp`. Returns the number of checks that passed out of
   `cxx_throw_total()`, so a partial result is distinct from a total failure. */
extern "C" int cxx_throw_run(void);
extern "C" int cxx_throw_total(void);

#endif /* CXX_THROW_H */
