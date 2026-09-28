/*
 * The engine's crash handler, answered here instead of by `engine/platform/posix/crash_posix.c`.
 *
 * `common/crashtrace.c` holds the fatal signals on this platform: it reads this platform's
 * register context and prints the payload addresses the `.map` turns into names. The engine's
 * POSIX handler would install over it and report a desktop backtrace through `libbacktrace`,
 * which is not built. So the engine's two calls do nothing, which is exactly what is wanted.
 *
 * C++ so that `common/cxx.mk` has a title source to build the C++ runtime beside (`cxxrt.cpp`:
 * `__cxa_atexit`, the static-init guards) - the engine's own C++ is in the relocatable libraries.
 */
extern "C" {
void Sys_SetupCrashHandler(const char *argv0);
void Sys_RestoreCrashHandler(void);
}

extern "C" void Sys_SetupCrashHandler(const char *argv0) {
    (void)argv0;
}

extern "C" void Sys_RestoreCrashHandler(void) {
}
