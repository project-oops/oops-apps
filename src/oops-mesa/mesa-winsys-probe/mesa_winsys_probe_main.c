/*
 * mesa-winsys-probe: the smallest title that links upstream Mesa, and a report of how
 * far it gets.
 *
 * # What this is for
 *
 * Every other check in oops-mesa is a compile. This one is a link: Mesa's 45 archives,
 * the winsys and runtime shims, the C++ support archive and oops-sdk, in one binary,
 * built the way a real title builds. If the shape of the SDK fragment is wrong, this is
 * what says so.
 *
 * # What it does not do
 *
 * Render. It walks radeonsi's startup path as far as that path goes and logs where it
 * stops.
 *
 * Until 2026-09-16 the stopping point was known in advance: radeonsi reads
 * `GB_ADDR_CONFIG` before it will finish initialising, and the winsys refused that
 * register rather than invent a value (oops-mesa worklog 010). The winsys now answers
 * it - not from a register read, which faults the GPU, but from a value derived against
 * oops-sdk's tiler, written out beside `OOPS_GB_ADDR_CONFIG` in oops-mesa's
 * `drm_device.c`.
 *
 * Since then the whole path has been traced on paper. oops-mesa worklog 021 tabulates
 * every call from libdrm's first statement to the end of `ac_query_gpu_info` with its
 * failure disposition, and worklog 022 carries it through to a screen: on this part,
 * creating one issues no further ioctl and allocates no GPU memory, so a screen
 * appearing here proves the whole chain without submitting anything.
 *
 * That is what makes this worth running. Not that the stopping point is unknown - it is
 * predicted now - but that nothing in the prediction has met the hardware, and every
 * answer the winsys gives is a value this collection decided rather than read off the
 * silicon.
 */

#include "oops/display.h"
#include "oops/memory.h"
#include "oops/system.h"
#include "oops/time.h"
#include "oops_winsys.h"

/*
 * Mesa's own headers, not copies of them - the option cache is a real type with a real
 * layout and this file has no business restating it.
 *
 * They are included with the conversion warnings off. This title compiles with
 * `-Wconversion -Wsign-conversion -Werror` and Mesa does not; upstream headers are held
 * to upstream's standard, not to ours. The suppression covers the includes and nothing
 * after them, so every line this file actually owns is still checked at the strict
 * setting.
 */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wstring-conversion"
#include "util/driconf.h"
#pragma clang diagnostic pop

#include <stdint.h>

static void say(const char *msg) {
    oops_klog("MESA-PROBE", msg);
}

/*
 * How this title finishes, which is by not finishing.
 *
 * The measurement and the reasoning live with the shared helper in
 * `oops-sdk/include/oops/system.h`; the short version is that no userland call
 * terminates a `big-app` process, because lifecycle belongs to `SceShellCore`, and
 * returning from the entry point faults at `rip: 0x0` for want of a caller frame
 * (obSCEne `REQ-20260917T1450Z-2e71`). That fault is the tail of every run this title
 * has ever made, including the successful one in
 * `oops-mesa/docs/hardware/screen-created-fw1240.md` - always *after* `done`, so it
 * cost no measurement, only a clean ending.
 *
 * The line is said here rather than in the helper so that it carries this title's own
 * tag, which is what a reader greps for. The idling is the helper's.
 */
_Noreturn static void park(void) {
    say("idle and finished - close this title from the host");
    oops_system_park_until_closed();
}

/*
 * radeonsi's entry point, declared here rather than included.
 *
 * Mesa's own header for it drags in the whole gallium tree, and a title has no business
 * seeing that. The signature is from `src/gallium/drivers/radeonsi/si_pipe.c` at the
 * pinned revision; `pipe_screen_config` is passed as a pointer this never dereferences,
 * so it stays opaque.
 */
struct pipe_screen;

/*
 * From `mesa/src/gallium/include/pipe/p_screen.h` at the pin. Restated here rather than
 * included for the reason the function below is: `p_screen.h` reaches the whole gallium
 * and util tree, including generated headers, and a title has no business seeing that.
 *
 * Restating a layout is the thing this file got wrong once, so it is worth saying what
 * makes it safe here and not before. The old mistake was not a wrong layout, it was a
 * null pointer where a structure was required. This is three fields with the pointer
 * types Mesa declares, in Mesa's order, and the offsets were confirmed against the
 * faulting instruction on hardware: the winsys read +0x8 for `options` and +0x10 for
 * `options_info`. The pin is what keeps it true; a Mesa bump is where to check it
 * again.
 */
struct pipe_screen_config {
    bool driver_name_is_inferred;
    struct driOptionCache *options;
    const struct driOptionCache *options_info;
};

extern struct pipe_screen *
radeonsi_screen_create(int fd, const struct pipe_screen_config *config);

/*
 * The screen configuration, and why it is a real object rather than a null.
 *
 * This used to pass `0`, on the stated assumption that radeonsi "never dereferences"
 * it. Hardware disagreed on 2026-09-16: the title loaded, opened the winsys, and took a
 * SIGSEGV reading address 0x8 - `config->options` - before it reached a single ioctl.
 *
 * So the options have to exist. They do not have to be *complete*: `findOption` walks
 * the hash table until it meets an empty entry, and `driQueryOptionb` then reads that
 * entry's zeroed value, so an option this list does not declare answers false rather
 * than faulting. What is fatal is a null cache, because `findOption` reads
 * `cache->tableSize` first.
 *
 * One option is declared, the only one anything on this path asks for:
 * `amdgpu_winsys.c` reads `radeonsi_zerovram` in `do_winsys_init`. False is both its
 * upstream default and the cheaper behaviour.
 *
 * `driParseConfigFiles` would read drirc files, but this Mesa is configured
 * `-Dxmlconfig=disabled`, so it reduces to initialising the cache from the info table.
 * Nothing here touches a filesystem.
 */
static const driOptionDescription mesa_winsys_probe_options[] = {
    DRI_CONF_SECTION_DEBUG DRI_CONF_OPT_B(
        radeonsi_zerovram, false, "Zero all VRAM allocations") DRI_CONF_SECTION_END};

#ifndef OOPS_APP_VERSION
#define OOPS_APP_VERSION "dev"
#endif

void mesa_winsys_probe_start(void);

void mesa_winsys_probe_start(void) {
    say("linking upstream Mesa and walking its startup path (v" OOPS_APP_VERSION ")");

    /*
     * The device. On Linux this is a file in /dev; here it is a token the winsys hands
     * out and recognises, and opening it is the one thing that checks the platform
     * graphics driver is bound at all.
     */
    int fd = oops_winsys_open();
    if (fd < 0) {
        say("the platform graphics driver is not bound; nothing further is possible");
        park();
    }
    say("winsys device opened");

    /*
     * Everything from here belongs to Mesa. It will ask the driver version, then
     * whether the accelerator works, then for the device description, then read
     * `GB_ADDR_CONFIG` - and then carry on into a part of the path nothing here has
     * watched.
     *
     * The winsys logs each command it refuses, under its own name, so the system log
     * names the step rather than leaving a null return to be guessed at.
     */
    static driOptionCache option_info;
    static driOptionCache option_cache;
    driParseOptionInfo(&option_info, mesa_winsys_probe_options,
                       (unsigned)(sizeof(mesa_winsys_probe_options) /
                                  sizeof(mesa_winsys_probe_options[0])));

    /*
     * Tell Mesa this process's name, using its own injection point, because the way it
     * would otherwise find out ends in a null dereference if this platform answers the
     * way it might.
     *
     * `driParseConfigFiles` needs an executable name to match the built-in driconf
     * database against. It takes the first of three: an already-injected name,
     * `MESA_DRICONF_EXECUTABLE_ OVERRIDE` from the environment, or
     * `util_get_process_name()`. There is no environment here, so without this call the
     * third is used - and on a FreeBSD target that is `getprogname()`, whose result is
     * passed straight through:
     *
     *     const char *program_name = getprogname();
     *     if (program_name) return strdup(program_name);
     *     return NULL;                                  (mesa/src/util/u_process.c)
     *
     * A null from there becomes `data->execName`, and `parseAppAttr` then does
     * `strcmp(exec, data->execName)` against every entry in the built-in database that
     * carries an `executable` attribute - hundreds of them, unguarded
     * (mesa/src/util/xmlconfig.c).
     *
     * `getprogname` does resolve on this platform, so the title loads. What it
     * *returns* in a title started by the system loader rather than by a shell is not
     * measured anywhere in the collection: an empty string is harmless and a null
     * crashes in the same function `getenv` already crashed this title in once
     * (oops-mesa worklog 019).
     *
     * Injecting the name removes the question rather than betting on it. The title
     * knows what it is called - `OOPS_APP_NAME` is its own build identity - so this is
     * the honest answer as well as the safe one, and `driInjectExecName` is upstream's
     * own seam for exactly this (its own tests use it).
     */
    driInjectExecName(OOPS_APP_NAME);

    /*
     * This is deliberately duplicated work. `radeonsi_screen_create` calls
     * `driParseConfigFiles` itself on this very cache, so doing it here means
     * `initOptionCache` allocates twice and the first allocation leaks - once, a few
     * hundred bytes, for the life of the process.
     *
     * It is worth that. The call is the canary: reaching the line below proves the
     * option path survived, which separates "died building the option cache" from "died
     * inside radeonsi" without needing either of them to say so. That distinction is
     * not free to obtain otherwise, because Mesa reports its own failures on file
     * descriptor 2, and on this leg that descriptor goes nowhere:
     * `REQ-20260917T0233Z-5c9d` measured `write(2)` returning the full byte count with
     * `errno` zero and **nothing** reaching the captured log, with `dup2` no help
     * because fd 1 is equally disconnected. oops-mesa's `stderr_to_klog.c` exists to
     * intercept that traffic and forward it, so Mesa's own account does arrive - but
     * through interception rather than through the descriptor, and this canary does not
     * depend on either.
     */
    driParseConfigFiles(&option_cache, &option_info,
                        &(driConfigFileParseParams){
                            .screenNum = 0,
                            .driverName = "radeonsi",
                        });
    say("screen options built");

    const struct pipe_screen_config config = {
        .driver_name_is_inferred = false,
        .options = &option_cache,
        .options_info = &option_info,
    };
    struct pipe_screen *screen = radeonsi_screen_create(fd, &config);

    if (screen) {
        say("radeonsi created a screen: the startup path is complete");
    } else {
        /*
         * Deliberately states no cause, and now also says why it might not be able to.
         *
         * Two kinds of failure end here and they look identical from this line. If
         * radeonsi stopped on something the winsys refused, there is a winsys line
         * above naming that command. If it stopped on something the winsys *answered* -
         * a value Mesa then rejected, which is most of oops-mesa worklog 021's table -
         * the winsys logged nothing, because from its side nothing went wrong. In that
         * case the only account is Mesa's own, and Mesa writes it to file descriptor 2
         * - which on this leg reaches nothing at all
         * (`REQ-20260917T0233Z-5c9d`, measured). What carries it instead is oops-mesa's
         * `stderr_to_klog.c`, which intercepts the stdio layer and `write` and forwards
         * to the system log, so those lines appear tagged `MESA` rather than
         * `MESA-PROBE`.
         *
         * So: read the winsys lines if there are any, read the `MESA` lines if there
         * are any, and treat the absence of both as a fact about where the failure was
         * rather than as no information.
         */
        say("radeonsi did not create a screen");
        say("if a winsys line above names a refused command, that command is the "
            "result");
        say("if none does, it stopped on an answer it was given - do not assume the "
            "reason");
    }

    oops_winsys_close(fd);
    say("done");
    park();
}
