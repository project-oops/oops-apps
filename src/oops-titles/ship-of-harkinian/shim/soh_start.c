/*
 * Payload entry point: builds a one-element command line and calls upstream's main
 * (upstream/soh/src/code/main.c), which is a C translation unit, so the name is
 * unmangled.
 *
 * The loader runs no main and passes no argv. argv[0] gives upstream something to
 * report and keeps argc > 0 for its option parsing; the ROM and archives are found by
 * path, not from argv.
 */
#include "oops/display.h"
#include "oops/draw.h"
#include "oops/freestd.h"
#include "oops/fs.h"
#include "oops/syscall.h"
#include "oops/system.h"

/* `common/crashtrace.c`: the payload addresses on the stack when a fatal signal
 * arrives, which is the only way to see past a fault raised inside a platform library.
 */
void oops_crashtrace_install(void);
#include "oops/time.h"
#include "oops/zip.h"
#include <SDL2/SDL.h>
#include <dirent.h>
#include <stdlib.h>

int main(int argc, char **argv);

/* app.mk derives the entry symbol from the app name; lld only warns on a mismatch. */
int ship_of_harkinian_start(const payload_args_t *args);

/*
 * Says which game data is present, before libultraship looks.
 *
 * The build ships soh.o2r, the port's own archive, and carries no game assets. oot.o2r
 * and oot-mq.o2r are converted from a ROM the player supplies, by this payload, on the
 * console - so a fresh install has neither, and an install with a ROM has neither until
 * the first run finishes converting it. libultraship answers a missing one with
 * SDL_ShowSimpleMessageBox ("Main OTR file not found"), and the same fact goes to the
 * log, where it can be read without a screen.
 *
 * It used to report and carry on, on the grounds that whether the game can start is
 * libultraship's answer to give. It does not give one: `OTRGlobals.cpp:295` passes
 * `allowEmptyPaths = true`, so the resource manager accepts having loaded nothing and
 * says nothing, and the engine goes on building a window and a GUI over resources that
 * are not there. What the player sees for that is not a message, it is a title that
 * disappears.
 *
 * So it answers here instead, where the question is already settled: no ROM archive
 * means no game, and the one useful thing left to do is say so on screen.
 *
 * Returns non-zero when there is game data to start with.
 */
static int soh_report_game_data(void);

/*
 * Whether a ROM the game could convert is sitting in the package directory.
 *
 * Upstream converts one itself - `OTRGlobals.cpp:645` searches for a ROM and hands it
 * to `CallZapd` on a worker thread, counting through `extractCount`/`totalExtract` -
 * and that path has been dead here only because ZAPD was not in the payload. It is now,
 * so a ROM is reason enough to start: the game does the rest.
 */
static int soh_have_rom(void) {
    static const char *const suffixes[] = {".z64", ".n64", ".v64"};
    DIR *dir = opendir(OOPS_POSIX_HOME);
    struct dirent *entry;
    int found = 0;

    if (dir == NULL) {
        oops_log_warn("SOH", "could not list %s to look for a ROM", OOPS_POSIX_HOME);
        return 0;
    }
    while (!found && (entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        size_t len = obs_strlen(name);
        size_t s;

        for (s = 0; s < sizeof(suffixes) / sizeof(suffixes[0]); s++) {
            const char *suf = suffixes[s];
            if (len >= 4u && name[len - 4] == suf[0] && name[len - 3] == suf[1] &&
                name[len - 2] == suf[2] && name[len - 1] == suf[3]) {
                oops_log_info("SOH", "found a ROM to convert: %s/%s", OOPS_POSIX_HOME,
                              name);
                found = 1;
                break;
            }
        }
    }
    closedir(dir);
    return found;
}

struct soh_unpack_ctx {
    oops_display_t *disp;
    const char *title;
    uint64_t last_flip_ms;
};

static void soh_unpack_draw_centered(oops_surface_t *surf, int y, const char *text,
                                     oops_color_t color, int scale) {
    int w = oops_draw_text_width(text, scale);
    (void)oops_draw_text(surf, ((int)surf->width - w) / 2, y, text, color, scale);
}

static void soh_unpack_progress(uint32_t current, uint32_t total, void *userdata) {
    struct soh_unpack_ctx *ctx = (struct soh_unpack_ctx *)userdata;
    uint64_t now;
    oops_surface_t surf;
    const int bar_x = 560;
    const int bar_y = 530;
    const int bar_w = 800;
    const int bar_h = 24;
    int filled_w;
    unsigned int pct;
    char count_str[64];

    if (!ctx || !ctx->disp) {
        return;
    }

    now = oops_time_get_ms();
    if (current != 0 && current != total && (now - ctx->last_flip_ms) < 33) {
        return;
    }
    ctx->last_flip_ms = now;

    surf = oops_display_get_surface(ctx->disp);
    if (!surf.pixels) {
        return;
    }

    oops_draw_clear(&surf, 0xFF0E1116u);
    soh_unpack_draw_centered(&surf, 380, ctx->title, 0xFFE8ECF1u, 4);
    soh_unpack_draw_centered(&surf, 450, "Preparing asset definitions...", 0xFF8A94A3u,
                             3);

    oops_draw_rect(&surf, bar_x - 2, bar_y - 2, bar_w + 4, bar_h + 4, 0xFF2E3440u);
    oops_draw_rect(&surf, bar_x, bar_y, bar_w, bar_h, 0xFF14181Fu);

    filled_w = (total > 0) ? (int)((uint64_t)current * (uint64_t)bar_w / total) : 0;
    if (filled_w > bar_w) {
        filled_w = bar_w;
    }
    if (filled_w > 0) {
        oops_draw_rect(&surf, bar_x, bar_y, filled_w, bar_h, 0xFFFFD23Cu);
    }

    pct = (total > 0) ? (unsigned int)((uint64_t)current * 100u / total) : 0;
    (void)oops_snprintf(count_str, sizeof(count_str),
                        "Unpacking assets: %u / %u (%u%%)", (unsigned)current,
                        (unsigned)total, pct);
    soh_unpack_draw_centered(&surf, 590, count_str, 0xFF8A94A3u, 2);
    soh_unpack_draw_centered(&surf, 650, "This happens once on first launch",
                             0xFF5A6473u, 2);

    (void)oops_display_flip(ctx->disp);
    (void)oops_system_pump_events();
}

/*
 * Put the asset definitions on disk, once, before anything looks for them.
 *
 * ZAPD does not read a cartridge and work out what is in it: it reads XML saying what
 * lives at which offset, per ROM version, and there are 7,700 of those files. They ship
 * as `assets.zip` because `pros restore` costs per file rather than per byte and the
 * console has no use for 7,700 files it reads once - see the `make package` section of
 * the Makefile.
 *
 * An on-screen progress bar is displayed during extraction so the user receives
 * continuous visual feedback rather than an unresponsive black screen.
 *
 * The marker is a stamp written *after* the extract returns, not the destination
 * directory. Using `assets/xml` for it was wrong in the way that matters: a run that
 * unpacked 7,680 of the 7,700 entries and then died left `assets/xml` there looking
 * complete, so the next run saw the marker, skipped the unpack, and went on to read
 * `assets/filelists` and `assets/Config_*.xml` that had never been written. A partial
 * unpack must not look like a finished one, and only the extractor's return value knows
 * the difference.
 *
 * `OTRGlobals.cpp:433` tests `installPath + "/assets"` and puts up "Extractor assets
 * not found" when it is missing, so this has to happen before `main`, not on the way
 * into a conversion.
 */
static void soh_ensure_assets(void) {
    char marker[256];
    char archive[256];
    struct soh_unpack_ctx ctx = {0};
    uint64_t started;
    int rc;

    if (oops_snprintf(marker, sizeof(marker), "%s/assets/.unpacked", OOPS_POSIX_HOME) <=
            0 ||
        oops_snprintf(archive, sizeof(archive), "%s/assets.zip", OOPS_POSIX_HOME) <=
            0) {
        return;
    }
    if (oops_fs_exists(marker)) {
        return;
    }
    if (!oops_fs_exists(archive)) {
        oops_log_error("SOH",
                       "%s is missing: it ships with the build and holds the XML "
                       "the converter reads",
                       archive);
        return;
    }

    if (!oops_display_any_open()) {
        ctx.disp = oops_display_open(OOPS_DISPLAY_BACKEND_AUTO, 1920, 1080);
    }
    ctx.title = "Ship of Harkinian";
    ctx.last_flip_ms = 0;

    oops_log_info("SOH", "unpacking asset definitions - this happens once");
    started = oops_time_get_ms();
    rc = oops_zip_extract_filter_progress(archive, OOPS_POSIX_HOME, NULL,
                                          soh_unpack_progress, &ctx);

    if (ctx.disp != NULL) {
        oops_display_close(ctx.disp);
        ctx.disp = NULL;
    }

    if (rc != OOPS_ZIP_OK) {
        oops_log_error("SOH",
                       "could not unpack %s (%d); a ROM cannot be converted "
                       "without it",
                       archive, rc);
        return;
    }
    /* The stamp, now that the extractor has said it finished. Its presence is the only
     * thing that makes the next run skip this, so it is written last and never earlier.
     */
    {
        int fd =
            oops_fs_open(marker, OOPS_O_WRONLY | OOPS_O_CREAT | OOPS_O_TRUNC, 0644);
        if (fd < 0) {
            oops_log_warn("SOH",
                          "unpacked, but could not write %s - the next launch will "
                          "unpack again",
                          marker);
        } else {
            oops_fs_close(fd);
        }
    }
    oops_log_info("SOH", "asset definitions unpacked in %llu ms",
                  (unsigned long long)(oops_time_get_ms() - started));
}

static int soh_report_game_data(void) {
    static const char *const archives[] = {"oot.o2r", "oot-mq.o2r"};
    char path[256];
    size_t i;
    int have_archive = 0;

    for (i = 0; i < sizeof(archives) / sizeof(archives[0]); i++) {
        if (oops_snprintf(path, sizeof(path), "%s/%s", OOPS_POSIX_HOME, archives[i]) >
                0 &&
            oops_fs_exists(path)) {
            oops_log_info("SOH", "game data: %s", path);
            have_archive = 1;
        }
    }
    if (have_archive) {
        return 1;
    }

    /* No archive, but a ROM is enough: upstream's extractor converts it on the way in,
     * once the definitions it reads are on disk. */
    if (soh_have_rom()) {
        oops_log_info("SOH", "no archive yet - the ROM will be converted on this run");
        soh_ensure_assets();
        return 1;
    }

    if (oops_snprintf(path, sizeof(path), "%s/%s", OOPS_POSIX_HOME, "soh.o2r") > 0 &&
        !oops_fs_exists(path)) {
        oops_log_error("SOH", "%s is missing: the port archive ships with the build",
                       path);
    }

    oops_log_error("SOH",
                   "no ROM and no archive - copy a .z64, .n64 or .v64 to "
                   "/data/homebrew/%s/, beside eboot.bin, and it is converted on the "
                   "next launch",
                   OOPS_APP_ID);
    return 0;
}

/*
 * The same sentence the log carries, on the screen.
 *
 * `SDL_ShowSimpleMessageBox` before `SDL_Init` is deliberate and is the path SDL
 * provides for it: with no video device it asks each bootstrap in turn, which reaches
 * this platform's driver, and that draws the message itself when the system dialog
 * cannot be used - there is no video-out session this early for a common dialog to
 * composite against.
 */
static void soh_say_no_rom(void) {
    char message[512];

    /*
     * The path the player can act on, not the one the process sees.
     *
     * `OOPS_POSIX_HOME` is `/app0`, which is where the package is mounted from inside
     * the sandbox and means nothing to somebody copying a file onto the console. The
     * same directory is `/data/homebrew/<title id>` from outside, and it is the one
     * `eboot.bin` sits in - so say that, and name the file beside it that they can
     * already see.
     */
    if (oops_snprintf(message, sizeof(message),
                      "No Ocarina of Time ROM was found.\n\n"
                      "Copy your own ROM here, beside eboot.bin:\n\n"
                      "    /data/homebrew/%s/\n\n"
                      "A .z64, .n64 or .v64 file - any name will do. It is converted "
                      "on the next launch, once. Extraction is quick; writing the "
                      "archive is not, and the whole conversion takes upwards of "
                      "twenty minutes. The progress bar keeps moving throughout - "
                      "let it finish.\n\n"
                      "The ROM is yours to supply; nothing else is missing.",
                      OOPS_APP_ID) <= 0) {
        return;
    }
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Game data not found", message,
                             NULL);
}

__attribute__((visibility("default"))) int
ship_of_harkinian_start(const payload_args_t *args) {
    static char arg0[] = "/app0/soh";
    char *argv[] = {arg0, 0};

    (void)args;

    oops_log_init(OOPS_APP_ID);

    /* Before anything that can fault. This title and Spaghetti Kart both end in a page
     * fault at address 0x8 on a thread named `libcxx`, which the system reports against
     * libkernel rather than against the call that reached it; `common/crashtrace.c`
     * prints the payload addresses left on the stack, which the `.map` turns into
     * names. */
    oops_crashtrace_install();

    /* The kernel log drops bursts and this start-up is one. The sink writes to a USB
       stick when one is mounted and reports nothing when none is: `/data` is outside
       the sandbox and `oops_fs_get_storage_dir` refuses it, because escaping unmounts
       /app0 and its assets. */
    (void)oops_log_enable_disk_sink(OOPS_APP_ID, 0);

    oops_log_info("SOH", "entry");

    /* libultraship resolves its config and save paths through HOME, and the environment
       starts empty on this platform. */
    (void)oops_fs_mkdir(OOPS_POSIX_HOME, 0755);
    if (setenv("HOME", OOPS_POSIX_HOME, 1) != 0) {
        oops_log_error("SOH", "HOME could not be set; settings will not persist");
    }

    /* Namespace-scope constructors: .init_array is not walked for this payload, and
       libc++ and libultraship both build dispatch tables in theirs. Idempotent. */
    oops_run_init_array();

    /* ShipUtils seeds from rand() here, std::random_device needing an entropy source
       this platform does not have. Unseeded that is a fixed sequence, so the clock
       stands in for the device. */
    srand((unsigned)oops_time_get_counter());

    /*
     * Bring SDL's game-controller subsystem up now, before upstream reaches `main`.
     *
     * libultraship initialises it in `osContInit`, which is the *game's* controller
     * set-up and runs long after `InitOTR` - and `InitOTR` is where the extraction
     * prompt ("No O2R files found. Generate one now?") is raised. On a desktop that gap
     * is invisible, because the prompt is answered with a mouse. Here it meant the
     * first thing a player saw could not be answered at all.
     *
     * Three separate symptoms all came from this one gap, which is why it is worth
     * doing rather than patching each: SDL never ran the pad driver's `Init`, so the
     * pad's game-controller mapping was never registered and ImGui - which reads
     * `SDL_CONTROLLER_*` only - saw nothing; `oops_input_set_keyboard_as_pad(0)` lives
     * in that same `Init`, so the keyboard stayed folded into port 0 and a phantom key
     * held `OOPS_BUTTON_LEFT` down for the life of the process; and the subsystem was
     * never marked live, so `SDL_PumpEvents` never polled the driver after the first
     * call.
     *
     * Asking for it here runs that `Init` once, early, and costs nothing when upstream
     * asks again later - `SDL_InitSubSystem` reference-counts.
     */
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
        oops_log_error("SOH", "SDL game controllers would not start: %s",
                       SDL_GetError());
    } else {
        oops_log_info("SOH", "SDL game controllers up before main: %d joystick(s)",
                      SDL_NumJoysticks());
    }

    /*
     * No ROM archive, no game. Say so and stop, rather than handing an engine that has
     * loaded nothing to `main` and letting it fault a few frames later with the player
     * looking at a title that vanished.
     */
    if (!soh_report_game_data()) {
        soh_say_no_rom();
        oops_log_info("SOH", "stopping: there is no game data to start with");
        /*
         * `exit`, not `return`. Returning from the entry point jumps to a null return
         * address - the fault after the message was drawn, `rip` of 0 - because the
         * loader calls this and has nowhere to go back to. `exit` parks the process
         * instead, which is the conforming ending for a big app on this platform.
         */
        exit(0);
    }

    return main(1, argv);
}
