/*
 * The payload entry point.
 *
 * A payload is called, not spawned: the loader runs no `main` and passes no argv. This
 * builds a command line and calls ioquake3's `main` in `code/sys/sys_main.c`.
 *
 * `baseq3r` is found by probing `/data/homebrew/<id>` and `/app0`, packed (`pak0.pk3`)
 * or loose (`default.cfg`), and the choice is logged.
 *
 * The `+set` arguments are what upstream's `run.sh` passes and `Com_Init` parses them
 * before any cvar is read: `fs_basepath` is the root found, `fs_homepath` is writable
 * (`HOME` is unset here), and `vm_*` 2 selects the bytecode interpreter, the only mode
 * a `NO_VM_COMPILED` build without loadable modules has. `make qvm` builds the modules.
 */
#include "oops/fs.h"
/* For `payload_args_t`, which the entry signature names. */
#include "oops/syscall.h"
#include "oops/system.h"

int main(int argc, char **argv);

/* Prototype for `-Wmissing-prototypes`. `q3rally_start`, not `q3_start`: `app.mk`
   derives the entry symbol from the app name, and lld only warns on a mismatch. */
int q3rally_start(const payload_args_t *args);

/* Enough for "<root>/baseq3r/default.cfg"; the roots below are short and fixed. */
#define Q3_PATH_MAX 256

/* Appends to `buf` from `off`, NUL-terminating. Returns the new length, or 0 if it
   would not fit. */
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
    /* A homebrew title's own directory first, then the package mount. */
    static const char *const roots[] = {"/data/homebrew/" OOPS_APP_ID, "/app0"};
    static char arg0[Q3_PATH_MAX];
    static char basepath[Q3_PATH_MAX];
    const char *root = 0;
    size_t i;

    char *argv[] = {arg0,
                    (char *)"+set",
                    (char *)"fs_basepath",
                    basepath,
                    (char *)"+set",
                    (char *)"fs_homepath",
                    (char *)OOPS_POSIX_HOME,
                    (char *)"+set",
                    (char *)"vm_game",
                    (char *)"2",
                    (char *)"+set",
                    (char *)"vm_cgame",
                    (char *)"2",
                    (char *)"+set",
                    (char *)"vm_ui",
                    (char *)"2",
                    0};

    (void)args;

    oops_log_init(OOPS_APP_ID);

    /*
     * The disk sink, because the kernel log drops bursts and ioquake3's startup is one.
     * `/data/<app id>` must exist: the sink falls back to it with no USB stick present.
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
         * Refused rather than started: the engine would fail in `Com_Init` on
         * `Couldn't load default.cfg`, which does not name the missing root.
         */
        oops_log_error("QRLY", "no baseq3r under %s or %s - the data is not deployed",
                       roots[0], roots[1]);
        return 1;
    }

    /* `Sys_SetBinaryPath` keeps `argv[0]`'s dirname, so any trailing name serves. */
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
