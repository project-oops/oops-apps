/*
 * The one thing ZAPD asks for that this platform does not answer.
 *
 * `ZAPD/CrashHandler.cpp` is the only one of its 114 sources that does not compile for
 * this target: it includes `<execinfo.h>` for `backtrace`, which is a desktop facility
 * and is not here. It is also not wanted. It installs its own handlers for the fatal
 * signals, and `common/crashtrace.c` already holds those - the last handler to install
 * wins, and ZAPD's would report a desktop backtrace for a fault that is not on a
 * desktop while the one that reads this platform's register context went quiet.
 *
 * So the file is excluded from the build (`SOH_ZAPD_EXCLUDE_RE`) and this answers the
 * single symbol `ZAPD/Main.cpp` calls. It is a stub that does nothing, which is exactly
 * what is wanted here, and saying so is better than a handler that pretends.
 */

void CrashHandler_Init();

void CrashHandler_Init() {
    /* Deliberately empty: `common/crashtrace.c` owns the fatal signals on this target.
     */
}
