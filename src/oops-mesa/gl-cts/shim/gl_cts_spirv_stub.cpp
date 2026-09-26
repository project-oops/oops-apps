/*
 * `glc::spirvUtils` without glslang, standing in for `glcSpirvUtils.cpp`. glslang and
 * spirv-tools are fetched by upstream's `external/fetch_sources.py`, which no sparse
 * path reaches, and are not ported.
 *
 * Every function throws `NotSupportedError`, which the `.qpa` records apart from Pass
 * and Fail, so no case reports a pass it did not earn. `checkGlSpirvSupported` keeps
 * upstream's driver query (`glcSpirvUtils.cpp:49`); when the driver does advertise
 * SPIR-V, the message names the missing toolchain instead.
 */
#include "glcSpirvUtils.hpp"

#include "gluContextInfo.hpp"
#include "gluRenderContext.hpp"
#include "tcuTestLog.hpp"

namespace glc {
namespace spirvUtils {

/* The toolchain that is absent, named once. Every message below ends with it. */
static const char *const NO_GLSLANG =
    "SPIR-V is not available in this build: glslang and spirv-tools are not ported "
    "(see shim/gl_cts_spirv_stub.cpp)";

void checkGlSpirvSupported(deqp::Context &m_context) {
    /* Upstream's, verbatim - see the note above. */
    bool is_at_least_gl_46 = (glu::contextSupports(
        m_context.getRenderContext().getType(), glu::ApiType::core(4, 6)));
    bool is_arb_gl_spirv =
        m_context.getContextInfo().isExtensionSupported("GL_ARB_gl_spirv");

    if ((!is_at_least_gl_46) && (!is_arb_gl_spirv))
        TCU_THROW(NotSupportedError, "GL 4.6 or GL_ARB_gl_spirv is not supported");

    /* The driver has it and this binary cannot drive it. */
    TCU_THROW(NotSupportedError, NO_GLSLANG);
}

glu::ShaderBinary makeSpirV(tcu::TestLog &log, glu::ShaderSource source,
                            SpirvVersion version) {
    DE_UNREF(log);
    DE_UNREF(source);
    DE_UNREF(version);
    TCU_THROW(NotSupportedError, NO_GLSLANG);
}

void spirvAssemble(glu::ShaderBinaryDataType &dst, const std::string &src) {
    DE_UNREF(dst);
    DE_UNREF(src);
    TCU_THROW(NotSupportedError, NO_GLSLANG);
}

void spirvDisassemble(std::string &dst, const glu::ShaderBinaryDataType &src) {
    DE_UNREF(dst);
    DE_UNREF(src);
    TCU_THROW(NotSupportedError, NO_GLSLANG);
}

bool spirvValidate(glu::ShaderBinaryDataType &dst, bool throwOnError) {
    DE_UNREF(dst);
    DE_UNREF(throwOnError);
    TCU_THROW(NotSupportedError, NO_GLSLANG);
}

/*
 * A text check, but its SPIR-V input comes from `makeSpirV`, which throws; returning
 * `false` would report a failed check for a case that never ran.
 */
bool verifyMappings(std::string glslSource, std::string spirVSource,
                    SpirVMapping &mappings, bool anyOf) {
    DE_UNREF(glslSource);
    DE_UNREF(spirVSource);
    DE_UNREF(mappings);
    DE_UNREF(anyOf);
    TCU_THROW(NotSupportedError, NO_GLSLANG);
}

} // namespace spirvUtils
} // namespace glc
