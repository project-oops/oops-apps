/*
 * The three things this title's link asks for and nothing on the platform answers.
 *
 * Each is here because the import check refused to write a manifest without it - and that
 * refusal is the whole reason they are visible at all. A payload links
 * `--unresolved-symbols=ignore-all`, so every one of these would otherwise have produced a
 * binary that runs until it reaches the call.
 *
 * **None of them is a stub that pretends.** Two report "not supported" in the way dEQP already
 * knows how to read, and the third is genuinely a no-op.
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
 * `tcuImageIO.cpp` is the one framework source excluded for a library rather than a header:
 * it reads PNG through libpng, which is not ported. `gluTexture.cpp` references both entry
 * points, so leaving the file out is not the same as leaving the symbols out.
 *
 * **Throwing `NotSupportedError` is the honest answer and the one the suite is built for.** A
 * dEQP test that cannot obtain a resource reports `NotSupported` in its result row and the run
 * continues; that is a true statement about this build, and it appears in the `.qpa` where a
 * reader will see it. Returning an empty texture would instead produce a *comparison* against
 * blank data, which is a pass or fail that means nothing.
 */
namespace tcu
{
namespace ImageIO
{

void loadPNG(TextureLevel &, const Archive &, const char *fileName)
{
    TCU_THROW(NotSupportedError,
              (std::string("PNG loading is not built into this title (libpng is not ported): ") +
               fileName)
                  .c_str());
}

void loadPKM(CompressedTexture &, const Archive &, const char *fileName)
{
    TCU_THROW(NotSupportedError,
              (std::string("PKM loading is not built into this title (libpng is not ported): ") +
               fileName)
                  .c_str());
}

} // namespace ImageIO
} // namespace tcu

/* ---------------------------------------------------------------- EGL config enumeration */

/*
 * Five `eglu` entry points, referenced by `glcConfigListEGL.cpp` and reachable from nothing.
 *
 * `CTS-Configs` asks the platform what framebuffer configurations it offers, and upstream asks
 * twice - once through EGL, once through WGL - because `getDefaultConfigList` is written to run
 * on whichever is there:
 *
 *     try { getConfigListEGL(...); } catch (const std::exception &e) { qpPrintf("No EGL configs enumerated: %s\n", ...); }
 *     try { getConfigListWGL(...); } catch (const std::exception &e) { ... }
 *
 * **On this platform the EGL attempt throws before it reaches any of these.**
 * `getDefaultEglConfigList` opens with a `dynamic_cast<tcu::EglPlatform&>(platform)`, and
 * `shim/tcuOopsPlatform.cpp` is not one, so `std::bad_cast` is raised and `glcConfigListEGL.cpp:165`
 * rethrows it as `tcu::Exception("Platform is not tcu::EglPlatform")`. The caller catches that,
 * prints its line, and carries on to the default config. That is upstream's designed behaviour
 * for a platform without EGL, and it is already correct here.
 *
 * **So why define them at all.** Because unreachable is not the same as absent, and this target
 * punishes the difference. A payload links `--unresolved-symbols=ignore-all`, so five undefined
 * C++ symbols produce no error; `make imports` then cannot resolve them against obSCEne's
 * corpus - they are nobody's platform library, they are dEQP's own C++ - and the manifest is the
 * gate the container is refused at. `oops-apps#D006` and the `.init_array` fault both say the
 * same thing in different words: on this target a name that resolves to nothing is a jump to
 * zero, and the only safe undefined symbol is one that is defined.
 *
 * They throw rather than return, because the day `tcuOopsPlatform.cpp` *does* implement
 * `tcu::EglPlatform` the cast will start succeeding, and these will start being called. A
 * `NotSupportedError` then says which piece is missing. A `return {}` would enumerate zero
 * configs and look exactly like a platform that has EGL and offers nothing.
 */
#include "egluUtil.hpp"

namespace eglu
{

static const char *const NO_EGL = "EGL is not implemented by this platform (shim/gl_cts_gaps.cpp)";

eglw::EGLDisplay getAndInitDisplay(NativeDisplay &, Version *)
{
    TCU_THROW(NotSupportedError, NO_EGL);
}

void terminateDisplay(const eglw::Library &, eglw::EGLDisplay)
{
    TCU_THROW(NotSupportedError, NO_EGL);
}

std::vector<eglw::EGLConfig> getConfigs(const eglw::Library &, eglw::EGLDisplay)
{
    TCU_THROW(NotSupportedError, NO_EGL);
}

bool hasExtension(const eglw::Library &, eglw::EGLDisplay, const std::string &)
{
    TCU_THROW(NotSupportedError, NO_EGL);
}

eglw::EGLint getConfigAttribInt(const eglw::Library &, eglw::EGLDisplay, eglw::EGLConfig, eglw::EGLint)
{
    TCU_THROW(NotSupportedError, NO_EGL);
}

} // namespace eglu

extern "C" {

/* ---------------------------------------------------------------- POSIX timers */

/*
 * `deTimer.c` drives dEQP's watchdog through `timer_create`/`timer_settime`, and the platform
 * has no POSIX timer API - obSCEne's corpus does not place any of the three.
 *
 * They fail rather than succeed silently, and that distinction matters: `deTimer_create` checks
 * the return and gives back null, `deTimerScheduleInterval` then reports failure, and the
 * watchdog is simply never armed. A run without a watchdog is a run where a hung test hangs the
 * title instead of being killed - which is worse than having one, and much better than believing
 * a timer is armed when it is not.
 */
/* The signatures are the sysroot's, from `<time.h>:132-136`, not invented: it declares all three
 * even though the platform implements none, so a mismatched definition is a compile error rather
 * than a quiet ABI difference. */
int timer_create(clockid_t clockid, struct sigevent *__restrict sevp, timer_t *__restrict timerid)
{
    (void)clockid;
    (void)sevp;
    (void)timerid;
    errno = ENOSYS;
    return -1;
}

int timer_settime(timer_t timerid, int flags, const struct itimerspec *__restrict new_value,
                  struct itimerspec *__restrict old_value)
{
    (void)timerid;
    (void)flags;
    (void)new_value;
    (void)old_value;
    errno = ENOSYS;
    return -1;
}

int timer_delete(timer_t timerid)
{
    (void)timerid;
    errno = ENOSYS;
    return -1;
}

/* ---------------------------------------------------------------- std::mutex */

/*
 * Mesa's ACO is C++ and was compiled against the sysroot's libc++, which has threads; this
 * title's libc++ is built with `_LIBCPP_HAS_THREADS 0`, so `std::mutex` is not declared and
 * `mutex.cpp` is not built. The three members ACO references therefore have no definition.
 *
 * Every other Mesa title gets them from `liboopsmesa_cxx.a`, which this one filters out of the
 * link because it duplicates the real `std::logic_error` family. `cxx_support.o` inside it
 * defines these three as no-ops - and also `__next_prime`, `__libcpp_verbose_abort`,
 * `__cxa_guard_acquire` and `__cxa_pure_virtual`, all of which now come from the real libc++ and
 * libc++abi. Taking the object back would reintroduce four duplicates to fix three symbols, so
 * the three are defined here instead.
 *
 * **A no-op lock is correct here rather than a compromise.** oops-mesa's runtime runs Mesa's
 * compiler on the calling thread; there is no second thread contending for these, and the same
 * reasoning is what `_LIBCPP_HAS_THREADS 0` already asserts for the rest of the library. If
 * threads are ever turned on for the hosted build, these go and `mutex.cpp` takes over - which
 * is the argument for doing it.
 *
 * Spelled as mangled names because the class they belong to is not declared in this
 * configuration: there is no `std::mutex` here to write a member definition for.
 */
void _ZNSt3__15mutex4lockEv(void *self)
{
    (void)self;
}

void _ZNSt3__15mutex6unlockEv(void *self)
{
    (void)self;
}

void _ZNSt3__15mutexD1Ev(void *self)
{
    (void)self;
}

void _ZNSt3__15mutexD2Ev(void *self)
{
    (void)self;
}

} /* extern "C" */
