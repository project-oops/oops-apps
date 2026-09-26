/*
 * `glcts::registerPackages()` - which test packages this build contains.
 *
 * Stands in for the excluded `glcTestPackageRegistry.cpp`. Every package is
 * constructed as upstream constructs it, with the same name, so results stay
 * comparable. The `KHR-*` ES packages stay and report `NotSupported` without ES.
 *
 * Absent: dEQP's own EGL, GLES2, GLES3, GLES31, GL45-GLES3 and GL45-GLES31 packages,
 * from the top-level `modules/egl` and `modules/gles*`, which `upstream.lock` does not
 * fetch. Adding those to `UPSTREAM_SPARSE` and registering them is the whole change;
 * the EGL-based ones need a `tcu::EglPlatform`.
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

namespace glcts {

static tcu::TestPackage *createConfigPackage(tcu::TestContext &testCtx) {
    return new glcts::ConfigPackage(testCtx, "CTS-Configs");
}

static tcu::TestPackage *createES2Package(tcu::TestContext &testCtx) {
    return new es2cts::TestPackage(testCtx, "KHR-GLES2");
}
static tcu::TestPackage *createES30Package(tcu::TestContext &testCtx) {
    return new es3cts::ES30TestPackage(testCtx, "KHR-GLES3");
}
static tcu::TestPackage *createES31Package(tcu::TestContext &testCtx) {
    return new es31cts::ES31TestPackage(testCtx, "KHR-GLES31");
}
static tcu::TestPackage *createESEXTPackage(tcu::TestContext &testCtx) {
    return new esextcts::ESEXTTestPackage(testCtx, "KHR-GLESEXT");
}
static tcu::TestPackage *createES32Package(tcu::TestContext &testCtx) {
    return new es32cts::ES32TestPackage(testCtx, "KHR-GLES32");
}

static tcu::TestPackage *
createNoDefaultCustomContextPackage(tcu::TestContext &testCtx) {
    return new glcts::NoDefaultContextPackage(testCtx, "KHR-NoContext");
}
static tcu::TestPackage *createSingleConfigGL43TestPackage(tcu::TestContext &testCtx) {
    return new glcts::SingleConfigGL43TestPackage(testCtx, "KHR-Single-GL43");
}
static tcu::TestPackage *createSingleConfigGL44TestPackage(tcu::TestContext &testCtx) {
    return new glcts::SingleConfigGL44TestPackage(testCtx, "KHR-Single-GL44");
}
static tcu::TestPackage *createSingleConfigGL45TestPackage(tcu::TestContext &testCtx) {
    return new glcts::SingleConfigGL45TestPackage(testCtx, "KHR-Single-GL45");
}
static tcu::TestPackage *createSingleConfigGL46TestPackage(tcu::TestContext &testCtx) {
    return new glcts::SingleConfigGL46TestPackage(testCtx, "KHR-Single-GL46");
}
static tcu::TestPackage *createSingleConfigES31TestPackage(tcu::TestContext &testCtx) {
    return new glcts::SingleConfigES31TestPackage(testCtx, "KHR-Single-GLES31");
}
static tcu::TestPackage *createSingleConfigES32TestPackage(tcu::TestContext &testCtx) {
    return new glcts::SingleConfigES32TestPackage(testCtx, "KHR-Single-GLES32");
}

static tcu::TestPackage *createGL30Package(tcu::TestContext &testCtx) {
    return new gl3cts::GL30TestPackage(testCtx, "KHR-GL30");
}
static tcu::TestPackage *createGL31Package(tcu::TestContext &testCtx) {
    return new gl3cts::GL31TestPackage(testCtx, "KHR-GL31");
}
static tcu::TestPackage *createGL32Package(tcu::TestContext &testCtx) {
    return new gl3cts::GL32TestPackage(testCtx, "KHR-GL32");
}
static tcu::TestPackage *createGL33Package(tcu::TestContext &testCtx) {
    return new gl3cts::GL33TestPackage(testCtx, "KHR-GL33");
}

static tcu::TestPackage *createGL40Package(tcu::TestContext &testCtx) {
    return new gl4cts::GL40TestPackage(testCtx, "KHR-GL40");
}
static tcu::TestPackage *createGL41Package(tcu::TestContext &testCtx) {
    return new gl4cts::GL41TestPackage(testCtx, "KHR-GL41");
}
static tcu::TestPackage *createGL42Package(tcu::TestContext &testCtx) {
    return new gl4cts::GL42TestPackage(testCtx, "KHR-GL42");
}
static tcu::TestPackage *createGL42CompatPackage(tcu::TestContext &testCtx) {
    return new gl4cts::GL42CompatTestPackage(testCtx, "KHR-GL42-COMPAT");
}
static tcu::TestPackage *createGL43Package(tcu::TestContext &testCtx) {
    return new gl4cts::GL43TestPackage(testCtx, "KHR-GL43");
}
static tcu::TestPackage *createGL44Package(tcu::TestContext &testCtx) {
    return new gl4cts::GL44TestPackage(testCtx, "KHR-GL44");
}
static tcu::TestPackage *createGL45Package(tcu::TestContext &testCtx) {
    return new gl4cts::GL45TestPackage(testCtx, "KHR-GL45");
}
static tcu::TestPackage *createGL46Package(tcu::TestContext &testCtx) {
    return new gl4cts::GL46TestPackage(testCtx, "KHR-GL46");
}

/*
 * Called by a constructor in upstream's `glcTestPackageEntry.cpp`, so it runs only when
 * `oops_mesa_run_init_array()` walks `.init_array`. The order is upstream's, so this
 * file diffs against `glcTestPackageRegistry.cpp`; the registry is keyed by name.
 */
void registerPackages(void) {
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
