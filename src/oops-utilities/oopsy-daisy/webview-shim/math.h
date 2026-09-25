/*
 * Freestanding <math.h> top-up for the webview build's C side (QuickJS).
 *
 * The SDK's freestanding libm has the core (exp/log/pow/sqrt/sin/cos/...), which this pulls in with
 * include_next; QuickJS's Math object also needs the hyperbolics and a few others the SDK libm does
 * not carry yet. They are composed from the core here - good enough for JS Math on a UI, and filed
 * for the SDK to provide properly (REQ-20260925T0110Z-e1d9).
 */
#ifndef OOPSY_SHIM_MATH_H
#define OOPSY_SHIM_MATH_H

#include_next <math.h>

static inline double cosh(double __x)  { double __e = exp(__x); return 0.5 * (__e + 1.0 / __e); }
static inline double sinh(double __x)  { double __e = exp(__x); return 0.5 * (__e - 1.0 / __e); }
static inline double tanh(double __x)  { double __e = exp(2.0 * __x); return (__e - 1.0) / (__e + 1.0); }
static inline double acosh(double __x) { return log(__x + sqrt(__x * __x - 1.0)); }
static inline double asinh(double __x) { return log(__x + sqrt(__x * __x + 1.0)); }
static inline double atanh(double __x) { return 0.5 * log((1.0 + __x) / (1.0 - __x)); }
static inline double cbrt(double __x)  { return __x < 0.0 ? -pow(-__x, 1.0 / 3.0) : pow(__x, 1.0 / 3.0); }
static inline double log1p(double __x) { return log(1.0 + __x); }
static inline double expm1(double __x) { return exp(__x) - 1.0; }

#endif /* OOPSY_SHIM_MATH_H */
