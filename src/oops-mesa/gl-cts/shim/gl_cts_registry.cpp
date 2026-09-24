/*
 * `glcts::registerPackages()` - which test packages this build contains.
 *
 * This stands in for `external/openglcts/modules/glcTestPackageRegistry.cpp`, which is excluded
 * from the build. Upstream's version registers thirty-one packages and this registers
 * twenty-five; the six it leaves out and the reason for each are below, and **the reason is
 * never that they were not wanted**.
 *
 * # Why a replacement rather than a patch
 *
 * `patches/` is for changing upstream's behaviour. This changes nothing about how any package
 * behaves - every package here is constructed exactly as upstream constructs it, with the same
 * name string, so a result from this binary is comparable with a result from any other. What
 * differs is only the *set*, and a set is better stated in one readable file than reconstructed
 * from a diff. `AGENTS.md` puts our code in `shim/`; this is ours.
 *
 * # The six that are absent, and what it would take to have them
 *
 *     dEQP-EGL           teglTestPackage.hpp     modules/egl
 *     dEQP-GLES2         tes2TestPackage.hpp     modules/gles2
 *     dEQP-GLES3         tes3TestPackage.hpp     modules/gles3
 *     dEQP-GLES31        tes31TestPackage.hpp    modules/gles31
 *     dEQP-GL45-GLES3    tgl45es3TestPackage.hpp     modules/gles3
 *     dEQP-GL45-GLES31   tgl45es31TestPackage.hpp    modules/gles31
 *
 * These are dEQP's *own* test modules - the top-level `modules/` trees, not
 * `external/openglcts/modules/` - and `upstream.lock` does not fetch them. They are carried in
 * the conformance binary because a Khronos submission requires them; they are not part of the
 * `KHR-*` suite and nothing in the `KHR-*` suite refers to them.
 *
 * **Adding them is four lines in the lock and no thought**, which is precisely why the shape of
 * this file matters: adding `modules/egl modules/gles2 modules/gles3 modules/gles31` to
 * `UPSTREAM_SPARSE` and un-commenting the six blocks below is the whole change. The last two are
 * the ones worth having first - `dEQP-GL45-GLES3` and `dEQP-GL45-GLES31` run the ES3 and ES31
 * test sets against a *desktop GL 4.5 context*, so they exercise this driver rather than an ES
 * driver it does not have. The first four need an EGL display, and
 * `shim/tcuOopsPlatform.cpp` implements no `tcu::EGLPlatform`, so they would construct and then
 * report `NotSupported` for every case.
 *
 * # Everything else upstream registers is here
 *
 * All twenty-four `KHR-*` packages and `CTS-Configs`, including the whole ES side. The ES
 * packages are kept even though this platform has no ES driver: `KHR-GLES2` and its siblings ask
 * the platform for an ES context, the platform does not offer one, and dEQP reports
 * `NotSupported` - which is a *result*, and the honest one. Dropping them would make the case
 * list shorter without making any answer in it different, and `oops-mesa`'s roadmap row 8 is
 * explicit that which cases run is a run-time choice. `/app0/cts-args.txt` is where that choice
 * is made.
 */
#include "glcTestPackageRegistry.hpp"

#include "glcConfigPackage.hpp"
#include "glcNoDefaultContextPackage.hpp"
#include "glcSingleConfigTestPackage.hpp"

#include "es2cTestPackage.hpp"
#include "es3cTestPackage.hpp"
#include "es31cTestPackage.hpp"
#include "es32cTestPackage.hpp"
#include "esextcTestPackage.hpp"

#include "gl3cTestPackages.hpp"
#include "gl4cTestPackages.hpp"

namespace glcts
{

static tcu::TestPackage *createConfigPackage(tcu::TestContext &testCtx)
{
    return new glcts::ConfigPackage(testCtx, "CTS-Configs");
}

static tcu::TestPackage *createES2Package(tcu::TestContext &testCtx)
{
    return new es2cts::TestPackage(testCtx, "KHR-GLES2");
}
static tcu::TestPackage *createES30Package(tcu::TestContext &testCtx)
{
    return new es3cts::ES30TestPackage(testCtx, "KHR-GLES3");
}
static tcu::TestPackage *createES31Package(tcu::TestContext &testCtx)
{
    return new es31cts::ES31TestPackage(testCtx, "KHR-GLES31");
}
static tcu::TestPackage *createESEXTPackage(tcu::TestContext &testCtx)
{
    return new esextcts::ESEXTTestPackage(testCtx, "KHR-GLESEXT");
}
static tcu::TestPackage *createES32Package(tcu::TestContext &testCtx)
{
    return new es32cts::ES32TestPackage(testCtx, "KHR-GLES32");
}

static tcu::TestPackage *createNoDefaultCustomContextPackage(tcu::TestContext &testCtx)
{
    return new glcts::NoDefaultContextPackage(testCtx, "KHR-NoContext");
}
static tcu::TestPackage *createSingleConfigGL43TestPackage(tcu::TestContext &testCtx)
{
    return new glcts::SingleConfigGL43TestPackage(testCtx, "KHR-Single-GL43");
}
static tcu::TestPackage *createSingleConfigGL44TestPackage(tcu::TestContext &testCtx)
{
    return new glcts::SingleConfigGL44TestPackage(testCtx, "KHR-Single-GL44");
}
static tcu::TestPackage *createSingleConfigGL45TestPackage(tcu::TestContext &testCtx)
{
    return new glcts::SingleConfigGL45TestPackage(testCtx, "KHR-Single-GL45");
}
static tcu::TestPackage *createSingleConfigGL46TestPackage(tcu::TestContext &testCtx)
{
    return new glcts::SingleConfigGL46TestPackage(testCtx, "KHR-Single-GL46");
}
static tcu::TestPackage *createSingleConfigES31TestPackage(tcu::TestContext &testCtx)
{
    return new glcts::SingleConfigES31TestPackage(testCtx, "KHR-Single-GLES31");
}
static tcu::TestPackage *createSingleConfigES32TestPackage(tcu::TestContext &testCtx)
{
    return new glcts::SingleConfigES32TestPackage(testCtx, "KHR-Single-GLES32");
}

static tcu::TestPackage *createGL30Package(tcu::TestContext &testCtx)
{
    return new gl3cts::GL30TestPackage(testCtx, "KHR-GL30");
}
static tcu::TestPackage *createGL31Package(tcu::TestContext &testCtx)
{
    return new gl3cts::GL31TestPackage(testCtx, "KHR-GL31");
}
static tcu::TestPackage *createGL32Package(tcu::TestContext &testCtx)
{
    return new gl3cts::GL32TestPackage(testCtx, "KHR-GL32");
}
static tcu::TestPackage *createGL33Package(tcu::TestContext &testCtx)
{
    return new gl3cts::GL33TestPackage(testCtx, "KHR-GL33");
}

static tcu::TestPackage *createGL40Package(tcu::TestContext &testCtx)
{
    return new gl4cts::GL40TestPackage(testCtx, "KHR-GL40");
}
static tcu::TestPackage *createGL41Package(tcu::TestContext &testCtx)
{
    return new gl4cts::GL41TestPackage(testCtx, "KHR-GL41");
}
static tcu::TestPackage *createGL42Package(tcu::TestContext &testCtx)
{
    return new gl4cts::GL42TestPackage(testCtx, "KHR-GL42");
}
static tcu::TestPackage *createGL42CompatPackage(tcu::TestContext &testCtx)
{
    return new gl4cts::GL42CompatTestPackage(testCtx, "KHR-GL42-COMPAT");
}
static tcu::TestPackage *createGL43Package(tcu::TestContext &testCtx)
{
    return new gl4cts::GL43TestPackage(testCtx, "KHR-GL43");
}
static tcu::TestPackage *createGL44Package(tcu::TestContext &testCtx)
{
    return new gl4cts::GL44TestPackage(testCtx, "KHR-GL44");
}
static tcu::TestPackage *createGL45Package(tcu::TestContext &testCtx)
{
    return new gl4cts::GL45TestPackage(testCtx, "KHR-GL45");
}
static tcu::TestPackage *createGL46Package(tcu::TestContext &testCtx)
{
    return new gl4cts::GL46TestPackage(testCtx, "KHR-GL46");
}

/*
 * Called from `external/openglcts/modules/glcTestPackageEntry.cpp`, which is upstream's and is
 * compiled unmodified. That file is a single namespace-scope object whose constructor calls
 * this - so **nothing here runs unless `.init_array` is walked**, and on this platform nothing
 * walks it but `oops_mesa_run_init_array()` in `shim/gl_cts_entry.c`. Without that call the
 * suite starts, opens its log, finds an empty registry and reports zero cases.
 *
 * The registration order is upstream's. It has no effect on execution - `tcu::TestPackageRegistry`
 * is keyed by name and the runner walks it by name - but keeping it makes this file diffable
 * against `glcTestPackageRegistry.cpp` on a version bump, which is the one thing that has to stay
 * cheap about carrying a replacement for somebody else's file.
 */
void registerPackages(void)
{
    tcu::TestPackageRegistry *registry = tcu::TestPackageRegistry::getSingleton();

    registry->registerPackage("CTS-Configs", createConfigPackage);

    /* dEQP-EGL - needs modules/egl */
    registry->registerPackage("KHR-GLES2", createES2Package);
    /* dEQP-GLES2 - needs modules/gles2 */

    registry->registerPackage("KHR-GLES3", createES30Package);
    /* dEQP-GLES3 - needs modules/gles3 */

    /* dEQP-GLES31 - needs modules/gles31 */
    /* dEQP-GL45-GLES31 - needs modules/gles31 */
    /* dEQP-GL45-GLES3 - needs modules/gles3 */
    registry->registerPackage("KHR-GLES31", createES31Package);
    registry->registerPackage("KHR-GLESEXT", createESEXTPackage);

    registry->registerPackage("KHR-GLES32", createES32Package);

    registry->registerPackage("KHR-NoContext", createNoDefaultCustomContextPackage);
    registry->registerPackage("KHR-Single-GL43", createSingleConfigGL43TestPackage);
    registry->registerPackage("KHR-Single-GL44", createSingleConfigGL44TestPackage);
    registry->registerPackage("KHR-Single-GL45", createSingleConfigGL45TestPackage);
    registry->registerPackage("KHR-Single-GL46", createSingleConfigGL46TestPackage);
    registry->registerPackage("KHR-Single-GLES31", createSingleConfigES31TestPackage);
    registry->registerPackage("KHR-Single-GLES32", createSingleConfigES32TestPackage);

    registry->registerPackage("KHR-GL30", createGL30Package);
    registry->registerPackage("KHR-GL31", createGL31Package);
    registry->registerPackage("KHR-GL32", createGL32Package);
    registry->registerPackage("KHR-GL33", createGL33Package);

    registry->registerPackage("KHR-GL40", createGL40Package);
    registry->registerPackage("KHR-GL41", createGL41Package);
    registry->registerPackage("KHR-GL42", createGL42Package);
    registry->registerPackage("KHR-COMPAT-GL42", createGL42CompatPackage);
    registry->registerPackage("KHR-GL43", createGL43Package);
    registry->registerPackage("KHR-GL44", createGL44Package);
    registry->registerPackage("KHR-GL45", createGL45Package);
    registry->registerPackage("KHR-GL46", createGL46Package);
}

} // namespace glcts
