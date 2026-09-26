/*
 * The dEQP platform layer for this console, the equivalent of a `framework/platform/`
 * directory. Everything else compiled here is upstream's.
 *
 * A `glu::RenderContext` provides `getType()`, `getFunctions()`, `getRenderTarget()`
 * and `postIterate()` (`gluRenderContext.hpp`); `oops_gfx_create` opens the display
 * and makes a context current in one call. Only the GL platform is implemented: there
 * is no EGL (oops-mesa#D010) or Vulkan, and the base class throws `NotSupportedError`
 * for them.
 */
#include "tcuPlatform.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuCommandLine.hpp"

#include "gluPlatform.hpp"
#include "gluRenderContext.hpp"
#include "gluContextFactory.hpp"
#include "gluRenderConfig.hpp"

#include "glwFunctions.hpp"
#include "glwFunctionLoader.hpp"

extern "C" {
#include "oops/gfx.h"
#include "oops/system.h"
}

namespace oops {

/* Defined in the generated `gl_loader_table.cpp`. */
glw::GenericFuncType glLoaderGet(const char *name);
unsigned glLoaderCount(void);

namespace {

/*
 * The loader is a table lookup: a static title has no `dlsym`, and this Mesa build
 * does not export `_glapi_get_proc_address`. `tools/gen-gl-loader.py` generates the
 * table from upstream's `glwInitGL33.inl` and what Mesa's `libglapi_bridge.a` defines.
 * It never returns null: an absent entry point is a stub that throws
 * `NotSupportedError`.
 */
class FunctionLoader : public glw::FunctionLoader {
  public:
    glw::GenericFuncType get(const char *name) const override {
        return glLoaderGet(name);
    }
};

class RenderContext : public glu::RenderContext {
  public:
    RenderContext(const glu::RenderConfig &config);
    ~RenderContext(void) override;

    glu::ContextType getType(void) const override { return m_type; }
    const glw::Functions &getFunctions(void) const override { return m_functions; }
    const tcu::RenderTarget &getRenderTarget(void) const override {
        return m_renderTarget;
    }

    void postIterate(void) override;

  private:
    glu::ContextType m_type;
    struct oops_gfx *m_gfx;
    glw::Functions m_functions;
    tcu::RenderTarget m_renderTarget;
};

/*
 * The requested size is passed to `oops_gfx_create` (0 means the display's own), but
 * the display may not honour it, so the real extent is read back and reported through
 * `getRenderTarget()`, which the tests size themselves against.
 */
RenderContext::RenderContext(const glu::RenderConfig &config)
    : m_type(config.type), m_gfx(nullptr),
      m_renderTarget(0, 0, tcu::PixelFormat(0, 0, 0, 0), 0, 0, 0) {
    const oops_gfx_desc_t desc = {
        static_cast<uint32_t>(config.width > 0 ? config.width : 0),
        static_cast<uint32_t>(config.height > 0 ? config.height : 0),
        true, /* depth: the CTS depth-buffer cases need one and it costs nothing here */
        true, /* vsync: a present that outruns scanout overruns the flip queue */
    };

    m_gfx = oops_gfx_create(&desc);
    if (m_gfx == nullptr)
        TCU_FAIL("oops_gfx_create failed: no GL context");

    uint32_t width = 0, height = 0;
    oops_gfx_extent(m_gfx, &width, &height);

    /* `oops_gfx_create` opens RGBA8888 with 24-bit depth and 8-bit stencil; dEQP
     * compares images against the format reported here. */
    m_renderTarget =
        tcu::RenderTarget(static_cast<int>(width), static_cast<int>(height),
                          tcu::PixelFormat(8, 8, 8, 8), 24, 8, 0);

    const FunctionLoader loader;
    glu::initCoreFunctions(&m_functions, &loader, m_type.getAPI());
    glu::initExtensionFunctions(&m_functions, &loader, m_type.getAPI());
}

RenderContext::~RenderContext(void) {
    if (m_gfx != nullptr)
        oops_gfx_destroy(m_gfx);
}

/* Called at the end of every test case. Presenting keeps the run visible on the panel,
 * where a hang differs from a slow test. There is no event queue to pump. */
void RenderContext::postIterate(void) {
    if (m_gfx != nullptr)
        oops_gfx_present(m_gfx);
}

class ContextFactory : public glu::ContextFactory {
  public:
    ContextFactory(void)
        : glu::ContextFactory(
              "oops", "OpenGL context through oops-mesa on the console display") {}

    glu::RenderContext *createContext(const glu::RenderConfig &config,
                                      const tcu::CommandLine &,
                                      const glu::RenderContext *) const override {
        return new RenderContext(config);
    }
};

class GLPlatform : public glu::Platform {
  public:
    GLPlatform(void) { m_contextFactoryRegistry.registerFactory(new ContextFactory()); }
};

class Platform : public tcu::Platform {
  public:
    const glu::Platform &getGLPlatform(void) const override { return m_glPlatform; }

  private:
    GLPlatform m_glPlatform;
};

} // namespace
} // namespace oops

/*
 * dEQP's entry point into the platform, declared by `tcuMain.cpp` and by every platform
 * directory upstream ships.
 */
tcu::Platform *createPlatform(void) {
    return new oops::Platform();
}

/*
 * A C-callable way into `tcuMain.cpp`'s `main`. Under `-ffreestanding`
 * (`common/cxx.mk`) `main` is an ordinary C++ function mangled as `_Z4mainiPPc`, which
 * `shim/gl_cts_entry.c` cannot name; compiled as C++, this wrapper can.
 */
int main(int argc, char **argv); /* upstream's, in tcuMain.cpp */

extern "C" int oops_cts_run_main(int argc, char **argv) {
    return main(argc, argv);
}
