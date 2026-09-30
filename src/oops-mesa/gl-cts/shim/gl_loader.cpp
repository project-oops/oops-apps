/*
 * The GL entry point lookup dEQP's `glw::FunctionLoader` performs.
 *
 * Mesa's own dispatch answers it: `_mesa_glapi_get_proc_address` is what
 * `glXGetProcAddress` and `wglGetProcAddress` call, and it resolves by name against the
 * stub table generated from `gl_and_es_API.xml`, so its coverage is the whole dispatch
 * rather than the subset `libGL.so` exports as linkable symbols.
 *
 * A name it does not know resolves to a stub that throws `tcu::NotSupportedError`,
 * which dEQP records as a case that did not run. Returning null instead puts address
 * zero in a function table, and the caller does not check.
 */
#include "glwDefs.hpp"
#include "glwFunctionLoader.hpp"
#include "glwFunctions.hpp"

#include "tcuDefs.hpp"

extern "C" {
#include "oops/system.h"

/* Restated from Mesa's `src/mesa/glapi/glapi/glapi.h`, which is not on this title's
   include path. */
typedef void (*_glapi_proc)(void);
_glapi_proc _mesa_glapi_get_proc_address(const char *funcName);
}

namespace oops {

namespace {

/* Cast to whatever the caller's table expects. It never returns, so no return value is
   read, and its arguments sit in caller-saved registers it ignores. */
void absent(void) {
    TCU_THROW(NotSupportedError, "this build has no entry point for that GL function");
}

} // namespace

glw::GenericFuncType glLoaderGet(const char *name) {
    const _glapi_proc p = _mesa_glapi_get_proc_address(name);

    if (p == nullptr) {
        /* Named here rather than in the stub: the loader asks once per function at
           context creation, so this reports every gap in one pass, while a stub reports
           only the first one a test happens to call. */
        oops_klog("OOPS-GL", name);
        return reinterpret_cast<glw::GenericFuncType>(absent);
    }

    return reinterpret_cast<glw::GenericFuncType>(p);
}

} // namespace oops
