/*
 * Symbols this title's link needs that nothing on the platform defines: image loading,
 * EGL config enumeration and POSIX timers. A payload links with unresolved symbols
 * ignored and the import manifest refuses them, so each is defined here. None pretends
 * to succeed: each throws `NotSupportedError` or fails with ENOSYS.
 */
#include "tcuDefs.hpp"
#include "tcuTexture.hpp"
#include "tcuCompressedTexture.hpp"
#include "tcuResource.hpp"

#include <errno.h>
#include <signal.h>
#include <time.h>

/* ---------------------------------------------------------------- image loading */

/*
 * `tcuImageIO.cpp` is excluded (libpng is not ported) but `gluTexture.cpp` references
 * both loaders. `NotSupportedError` becomes a `NotSupported` result row; an empty
 * texture would give a meaningless comparison.
 */
namespace tcu {
namespace ImageIO {

void loadPNG(TextureLevel &, const Archive &, const char *fileName) {
    TCU_THROW(
        NotSupportedError,
        (std::string(
             "PNG loading is not built into this title (libpng is not ported): ") +
         fileName)
            .c_str());
}

void loadPKM(CompressedTexture &, const Archive &, const char *fileName) {
    TCU_THROW(
        NotSupportedError,
        (std::string(
             "PKM loading is not built into this title (libpng is not ported): ") +
         fileName)
            .c_str());
}

} // namespace ImageIO
} // namespace tcu

/* ---------------------------------------------------------------- EGL config
 * enumeration */

/*
 * `eglu` entry points referenced by `glcConfigListEGL.cpp`. They are unreachable: the
 * platform is not a `tcu::EglPlatform`, so the cast fails first
 * (`glcConfigListEGL.cpp:165`) and `CTS-Configs` carries on. They are defined because
 * `make imports` cannot place dEQP's own symbols. They throw, so an EGL platform that
 * reaches them reports what is missing instead of enumerating zero configs.
 */
#include "egluUtil.hpp"

namespace eglu {

static const char *const NO_EGL =
    "EGL is not implemented by this platform (shim/gl_cts_gaps.cpp)";

eglw::EGLDisplay getAndInitDisplay(NativeDisplay &, Version *) {
    TCU_THROW(NotSupportedError, NO_EGL);
}

void terminateDisplay(const eglw::Library &, eglw::EGLDisplay) {
    TCU_THROW(NotSupportedError, NO_EGL);
}

std::vector<eglw::EGLConfig> getConfigs(const eglw::Library &, eglw::EGLDisplay) {
    TCU_THROW(NotSupportedError, NO_EGL);
}

bool hasExtension(const eglw::Library &, eglw::EGLDisplay, const std::string &) {
    TCU_THROW(NotSupportedError, NO_EGL);
}

eglw::EGLint getConfigAttribInt(const eglw::Library &, eglw::EGLDisplay,
                                eglw::EGLConfig, eglw::EGLint) {
    TCU_THROW(NotSupportedError, NO_EGL);
}

} // namespace eglu

extern "C" {

/* ---------------------------------------------------------------- POSIX timers */

/*
 * `deTimer.c` drives dEQP's watchdog through POSIX timers, which the platform lacks.
 * They fail with ENOSYS, so the watchdog is never armed and a hung test hangs the
 * title. The signatures match the sysroot's `<time.h>` declarations.
 */
int timer_create(clockid_t clockid, struct sigevent *__restrict sevp,
                 timer_t *__restrict timerid) {
    (void)clockid;
    (void)sevp;
    (void)timerid;
    errno = ENOSYS;
    return -1;
}

int timer_settime(timer_t timerid, int flags,
                  const struct itimerspec *__restrict new_value,
                  struct itimerspec *__restrict old_value) {
    (void)timerid;
    (void)flags;
    (void)new_value;
    (void)old_value;
    errno = ENOSYS;
    return -1;
}

int timer_delete(timer_t timerid) {
    (void)timerid;
    errno = ENOSYS;
    return -1;
}

} /* extern "C" */
