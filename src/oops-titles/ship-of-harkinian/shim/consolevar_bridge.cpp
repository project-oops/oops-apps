/*
 * C-to-C++ bridge for ConsoleVariable_SetInteger.
 *
 * The payload entry point (`soh_start.c`) is a C translation unit that needs to
 * set `gSettings.ControlNav` before `main()` runs. libultraship's ConsoleVariable
 * system is C++, so this thin bridge provides an `extern "C"` entry point that
 * the C shim can call.
 */
#include "ship/config/ConsoleVariable.h"
#include "ship/Context.h"

extern "C" void ConsoleVariable_SetInteger(const char *name, int32_t value) {
    auto ctx = Ship::Context::GetInstance();
    if (!ctx)
        return;
    auto cv = ctx->GetConsoleVariables();
    if (cv) {
        cv->SetInteger(name, value);
    }
}
