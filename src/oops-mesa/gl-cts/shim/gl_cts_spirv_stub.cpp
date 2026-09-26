/*
 * `glc::spirvUtils` without glslang - what the `GL_ARB_gl_spirv` tests get here, and
 * why it is still an honest answer.
 *
 * This stands in for `external/openglcts/modules/common/glcSpirvUtils.cpp`, the one
 * file in the whole test tree that cannot be compiled from what `upstream.lock`
 * fetches. It needs `glslang/Public/ShaderLang.h`, `SPIRV/GlslangToSpv.h` and
 * `spirv-tools/optimizer.hpp` - a GLSL front end and a SPIR-V assembler, validator and
 * disassembler.
 *
 * **Those are not in VK-GL-CTS.** Upstream pulls them with `external/fetch_sources.py`
 * into `external/glslang/src` and `external/spirv-tools/src`, so no `UPSTREAM_SPARSE`
 * path can ask for them; the lock's own fetch would have to grow a second origin. They
 * are also two more C++ ports to this target, which is a piece of work in its own right
 * and not one the GL suite is blocked on.
 *
 * # What the tests do instead
 *
 * Six functions, and every one of them reports `NotSupported`. That is dEQP's own
 * vocabulary for "this case did not run", it is recorded distinctly from `Pass` and
 * from `Fail` in the `.qpa` log, and `gl4cGlSpirvTests.cpp` - the only consumer besides
 * `glcSubgroupsTestsUtils.cpp` - calls `checkGlSpirvSupported()` first and is written
 * to have it throw.
 *
 * **So no case here can report a pass it did not earn**, which is the property that
 * matters. A table generated from the log will show these as not-run with the reason
 * attached, and a conformance submission built on this binary would be short exactly
 * these cases and say so.
 *
 * # `checkGlSpirvSupported` is upstream's code, deliberately
 *
 * The body below is copied from `glcSpirvUtils.cpp:49` unchanged, because it is a pure
 * GL query with no glslang in it and it is the *correct* answer whenever the driver
 * does not advertise the feature. On a driver without `GL_ARB_gl_spirv` and below
 * GL 4.6 - which is where oops-mesa is today - this file is behaviourally identical to
 * upstream's and the result is the result.
 *
 * The extra throw after it only fires once the driver *does* advertise SPIR-V, and then
 * it names the missing toolchain rather than the missing extension. That distinction is
 * the whole point of writing it this way round: the day oops-mesa reaches GL 4.6, these
 * cases have to stop saying "the driver cannot" and start saying "this build cannot",
 * or the first 4.6 run would quietly report the same not-supported it always had.
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

    /* The driver has it and this binary cannot drive it. A different sentence on
     * purpose. */
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
 * The one that does not obviously have to throw. It compares a GLSL source against a
 * SPIR-V one and is a pure text check - but the SPIR-V it is handed comes from
 * `makeSpirV`, which has already thrown, so there is no path that reaches this with
 * anything to compare. Returning `false` here would report a *failed mapping check* for
 * a case that never ran.
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
