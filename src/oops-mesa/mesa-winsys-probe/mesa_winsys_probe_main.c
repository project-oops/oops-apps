/*
 * mesa-winsys-probe: the smallest title that links upstream Mesa, and a report of how
 * far radeonsi's start-up path gets. It does not render.
 *
 * It opens the winsys device, builds the screen options, and calls
 * `radeonsi_screen_create`, logging each step under MESA-PROBE. Creating a screen
 * issues no GPU submission, so a screen proves the winsys chain on its own. Commands
 * the winsys refuses are logged by the winsys; Mesa's own messages arrive tagged MESA
 * through oops-mesa's `stderr_to_klog.c`.
 */

#include "oops/display.h"
#include "oops/memory.h"
#include "oops/system.h"
#include "oops/time.h"
#include "oops_winsys.h"

/* Mesa's own option-cache header, with the conversion warnings Mesa does not build
 * under suppressed for the include only. */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wstring-conversion"
#include "util/driconf.h"
#pragma clang diagnostic pop

#include <stdint.h>

#define TAG "MESA-PROBE"

/* A big-app cannot exit; it logs a last line, then idles until the host closes it. */
_Noreturn static void park(void) {
    oops_log_info(TAG, "idle and finished - close this title from the host");
    oops_system_park_until_closed();
}

/*
 * radeonsi's entry point and its config, declared here because Mesa's headers reach the
 * whole gallium tree. The signature is from `src/gallium/drivers/radeonsi/si_pipe.c`.
 */
struct pipe_screen;

/*
 * From `mesa/src/gallium/include/pipe/p_screen.h` at the pinned revision: the winsys
 * reads `options` at +0x8 and `options_info` at +0x10. Recheck on a Mesa bump.
 */
struct pipe_screen_config {
    bool driver_name_is_inferred;
    struct driOptionCache *options;
    const struct driOptionCache *options_info;
};

extern struct pipe_screen *
radeonsi_screen_create(int fd, const struct pipe_screen_config *config);

/*
 * The screen options. radeonsi dereferences `config->options`, so the cache must exist;
 * an undeclared option reads as false. The one declared is what `do_winsys_init` in
 * `amdgpu_winsys.c` reads. Mesa is built `-Dxmlconfig=disabled`, so no drirc is read.
 */
static const driOptionDescription mesa_winsys_probe_options[] = {
    DRI_CONF_SECTION_DEBUG DRI_CONF_OPT_B(
        radeonsi_zerovram, false, "Zero all VRAM allocations") DRI_CONF_SECTION_END};

#ifndef OOPS_APP_VERSION
#define OOPS_APP_VERSION "dev"
#endif

void mesa_winsys_probe_start(void);

void mesa_winsys_probe_start(void) {
    oops_log_info(
        TAG,
        "linking upstream Mesa and walking its startup path (v" OOPS_APP_VERSION ")");

    /* The device is a token the winsys hands out; opening it checks that the platform
     * graphics driver is bound. */
    int fd = oops_winsys_open();
    if (fd < 0) {
        oops_log_info(
            TAG,
            "the platform graphics driver is not bound; nothing further is possible");
        park();
    }
    oops_log_info(TAG, "winsys device opened");

    /* From here on Mesa drives. The winsys logs each command it refuses under its own
     * name. */
    static driOptionCache option_info;
    static driOptionCache option_cache;
    driParseOptionInfo(&option_info, mesa_winsys_probe_options,
                       (unsigned)(sizeof(mesa_winsys_probe_options) /
                                  sizeof(mesa_winsys_probe_options[0])));

    /*
     * `driParseConfigFiles` matches an executable name against the built-in driconf
     * database. Without an injected name it uses `getprogname()`, and a null there
     * reaches an unguarded `strcmp` in `parseAppAttr` (mesa/src/util/xmlconfig.c).
     */
    driInjectExecName(OOPS_APP_NAME);

    /*
     * `radeonsi_screen_create` parses this cache again, leaking the first allocation
     * once. The log line below separates a failure building the options from one
     * inside radeonsi.
     */
    driParseConfigFiles(&option_cache, &option_info,
                        &(driConfigFileParseParams){
                            .screenNum = 0,
                            .driverName = "radeonsi",
                        });
    oops_log_info(TAG, "screen options built");

    const struct pipe_screen_config config = {
        .driver_name_is_inferred = false,
        .options = &option_cache,
        .options_info = &option_info,
    };
    struct pipe_screen *screen = radeonsi_screen_create(fd, &config);

    if (screen) {
        oops_log_info(TAG, "radeonsi created a screen: the startup path is complete");
    } else {
        /*
         * No cause is stated: a refused command has a winsys line above, and a value
         * Mesa rejected has only Mesa's own lines, tagged MESA.
         */
        oops_log_info(TAG, "radeonsi did not create a screen");
        oops_log_info(
            TAG, "if a winsys line above names a refused command, that command is the "
                 "result");
        oops_log_info(
            TAG,
            "if none does, it stopped on an answer it was given - do not assume the "
            "reason");
    }

    oops_winsys_close(fd);
    oops_log_info(TAG, "done");
    park();
}
