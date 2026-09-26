/*
 * The payload entry point.
 *
 * A payload is called rather than spawned: there is no `main` the loader runs and no argv to hand
 * it. ioquake3's own `main` is in `code/sys/sys_main.c`, so this builds a command line and calls
 * it - which is the whole adaptation, and the reason this is a short file rather than a patch.
 *
 * # Finding `baseq3r`, by looking rather than by deciding
 *
 * Two titles in this collection disagree about where a package's files are, and both are right
 * about themselves: Neverball reads `/app0` and works, while Craft measured that a homebrew title
 * **cannot open `/app0` at all** - `oops_fs_opendir("/app0")` fails - and its assets are at
 * `/data/homebrew/CRFT00001`, where its own eboot was loaded from. Rather than guess which this
 * title is, or hard-code the answer and rediscover it on the console, this probes both and uses
 * the one that is actually there. The choice is logged, so a run that cannot find its data says
 * which paths it tried.
 *
 * Either data layout is accepted, because both are real:
 *
 *   <root>/baseq3r/pak0.pk3    packed, which is how Quake 3 data normally ships - and what this
 *                              title wants, because `pros restore` costs per *file*: the loose
 *                              tree is 3,623 of them, and one archive is one.
 *   <root>/baseq3r/default.cfg loose, which is how q3rally's repository ships it.
 *
 * # `+set`, not `argv`
 *
 * `main` concatenates argv into one command line for `Com_Init`, which parses `+set` before
 * anything reads a cvar - so these are the engine's own configuration mechanism rather than a
 * shim's back door, and they are exactly what upstream's `run.sh` passes:
 *
 *   fs_basepath   the root found above. `Sys_SetDefaultInstallPath(DEFAULT_BASEDIR)` would
 *                 otherwise derive it from `dirname(argv[0])`, which is also set correctly here -
 *                 this makes it explicit rather than relying on that.
 *   fs_homepath   somewhere writable, for `q3config.cfg` and screenshots. A package root may not
 *                 be, and the default answer comes from `getenv("HOME")`, which is nothing here.
 *   vm_game,      **2, meaning the bytecode interpreter.** 0 would `dlopen` a shared library and
 *   vm_cgame,     1 would compile the bytecode to x86 at runtime; the build is `NO_VM_COMPILED`
 *   vm_ui         and this target has no loadable modules, so 2 is the only one of the three that
 *                 can work. Upstream's own launcher passes the same.
 *
 * The game logic is three `.qvm` files under `baseq3r/vm/` and they are **not** built by the
 * payload build: they are bytecode, identical on every platform, produced by upstream's own
 * toolchain. `make qvm` in this title builds them; without them the engine starts and then fails
 * in `VM_Create`, which reads like a missing file because it is one.
 */
#include "oops/fs.h"
/* For `payload_args_t`, which the entry signature names. */
#include "oops/syscall.h"
#include "oops/system.h"

int main(int argc, char **argv);

/* Declared before it is defined because this file is ours and is held to the repository's warning
   set - `-Wmissing-prototypes` among them - unlike upstream's sources. */
/* `q3rally_start`, not `q3_start`: `app.mk` derives the entry symbol from the app's name
   (`ENTRY_POINT ?= $(subst -,_,$(APP_NAME))_start`), and a mismatch is a *warning* from lld -
   "cannot find entry symbol ...; not setting start address" - followed by a payload whose entry
   point is the start of the text segment. */
int q3rally_start(const payload_args_t *args);

/* Enough for "<root>/baseq3r/default.cfg" with room to spare; the roots below are short and
   fixed. */
#define Q3_PATH_MAX 256

/* Appends to `buf` from `off`, NUL-terminating. Returns the new length, or 0 if it would not fit -
   which the caller treats as "this root cannot be formed", not as a truncated path to try. */
static size_t q3_append(char *buf, size_t cap, size_t off, const char *s) {
    size_t i = 0;
    while (s[i] != '\0') {
        if (off + i + 1u >= cap) {
            return 0;
        }
        buf[off + i] = s[i];
        i++;
    }
    buf[off + i] = '\0';
    return off + i;
}

/* Whether `root` holds a `baseq3r` this engine could load, in either layout. */
static int q3_root_has_data(const char *root) {
    static const char *const probes[] = {"/baseq3r/pak0.pk3", "/baseq3r/default.cfg"};
    char path[Q3_PATH_MAX];
    size_t base;
    size_t i;

    base = q3_append(path, sizeof(path), 0u, root);
    if (base == 0u) {
        return 0;
    }
    for (i = 0; i < sizeof(probes) / sizeof(probes[0]); i++) {
        if (q3_append(path, sizeof(path), base, probes[i]) != 0u &&
            oops_fs_exists(path)) {
            oops_log_info("QRLY", "data found: %s", path);
            return 1;
        }
    }
    return 0;
}

__attribute__((visibility("default"))) int q3rally_start(const payload_args_t *args) {
    /* Craft's finding, and Neverball's, in the order that costs least when both are present: a
     * homebrew title's own directory first, then the conventional package mount. */
    static const char *const roots[] = {"/data/homebrew/" OOPS_APP_ID, "/app0"};
    static char arg0[Q3_PATH_MAX];
    static char basepath[Q3_PATH_MAX];
    const char *root = 0;
    size_t i;

    char *argv[] = {arg0,
                    (char *)"+set", (char *)"fs_basepath", basepath,
                    (char *)"+set", (char *)"fs_homepath", (char *)OOPS_POSIX_HOME,
                    (char *)"+set", (char *)"vm_game",  (char *)"2",
                    (char *)"+set", (char *)"vm_cgame", (char *)"2",
                    (char *)"+set", (char *)"vm_ui",    (char *)"2",
                    0};

    (void)args;

    oops_log_init(OOPS_APP_ID);

    /*
     * **The disk sink, because the kernel log drops bursts.** ioquake3's startup is a burst: the
     * filesystem lists every pk3 it finds, then the renderer prints its extension string. A run
     * that fails in `Com_Init` is exactly the run whose reason gets lost in transport - which is
     * what cost Craft a run before its sink was turned on. `/data/<app id>` has to exist first,
     * because that is where the sink falls back to when no USB stick is present.
     *
     * Unconditional here, unlike Craft's `verbose=1` gate, because this title has never started:
     * the first runs are the ones that need the log, and the level is still the compiled-in
     * default so this is not the debug-level firehose that cost Neverball 174ms of a 200ms frame.
     */
    (void)oops_fs_mkdir("/data/" OOPS_APP_ID, 0755);
    (void)oops_log_enable_disk_sink(OOPS_APP_ID, 0);

    oops_log_info("QRLY", "entry");

    /* Writable, for `q3config.cfg` - nothing creates it on a first run. */
    (void)oops_fs_mkdir(OOPS_POSIX_HOME, 0755);

    for (i = 0; i < sizeof(roots) / sizeof(roots[0]); i++) {
        if (q3_root_has_data(roots[i])) {
            root = roots[i];
            break;
        }
    }
    if (root == 0) {
        /*
         * **Refused rather than started.** Without `baseq3r` the engine gets as far as
         * `Com_Init` and dies on `Couldn't load default.cfg`, which names a file and not the
         * reason - the reason is that the root it looked under does not exist. Saying so here,
         * with both candidates spelled out, is the difference between a five-minute answer and a
         * console run spent reading someone else's error message.
         */
        oops_log_error("QRLY", "no baseq3r under %s or %s - the data is not deployed",
                       roots[0], roots[1]);
        return 1;
    }

    /* `argv[0]`'s *dirname* is what `Sys_SetBinaryPath` keeps and `DEFAULT_BASEDIR` derives the
     * install path from, so the trailing name matters only in that there has to be one. */
    {
        const size_t n = q3_append(arg0, sizeof(arg0), 0u, root);
        if (n == 0u || q3_append(arg0, sizeof(arg0), n, "/q3rally") == 0u ||
            q3_append(basepath, sizeof(basepath), 0u, root) == 0u) {
            oops_log_error("QRLY", "root path too long: %s", root);
            return 1;
        }
    }

    oops_log_info("QRLY", "fs_basepath=%s fs_homepath=%s", basepath, OOPS_POSIX_HOME);

    return main((int)(sizeof(argv) / sizeof(argv[0])) - 1, argv);
}
