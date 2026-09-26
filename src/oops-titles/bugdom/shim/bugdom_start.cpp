/*
 * Payload entry point: builds a one-element command line and calls upstream's main in
 * src/Boot.cpp. The loader runs no main and passes no argv.
 *
 * C++, not C: in a freestanding C++ translation unit main is an ordinary function, so
 * Boot.cpp exports _Z4mainiPPc and a C shim asking for main links - the payload link
 * ignores unresolved symbols - and faults at the first instruction.
 *
 * argv[0] is the whole interface. FindGameData (src/Boot.cpp:35) takes
 * parent_path(argv[0]) / "Data" on a non-Apple build, accepting it only when
 * Data/Skeletons/DoodleBug.3dmf opens, so this probes for that file under each
 * candidate root and hands back the one that answered.
 */
#include "oops/fs.h"
#include "oops/syscall.h"
#include "oops/system.h"
/* setenv; the environment starts empty, see oops-sdk/include/libc/stdlib.h. */
#include <stdlib.h>

/* Mangled, per the header comment. */
int main(int argc, char **argv);

/* Fits "<root>/Data/Skeletons/DoodleBug.3dmf"; the roots below are short and fixed. */
static const size_t BUG_PATH_MAX = 256;

/* Appends to buf from off, NUL-terminating. Returns the new length, or 0 if it would
 * not fit. */
static size_t bug_append(char *buf, size_t cap, size_t off, const char *s) {
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

/* The same file FindGameData checks: a probe for Data/ alone would accept a package
   whose assets failed to copy, and upstream would then throw. */
static int bug_root_has_data(const char *root) {
    char path[BUG_PATH_MAX];

    const size_t base = bug_append(path, sizeof(path), 0u, root);
    if (base == 0u) {
        return 0;
    }
    if (bug_append(path, sizeof(path), base, "/Data/Skeletons/DoodleBug.3dmf") == 0u) {
        return 0;
    }
    if (!oops_fs_exists(path)) {
        return 0;
    }
    oops_log_info("BUGD", "data found: %s", path);
    return 1;
}

extern "C" __attribute__((visibility("default"))) int
bugdom_start(const payload_args_t *args) {
    /* A homebrew title's own directory, then the package mount. */
    static const char *const roots[] = {"/data/homebrew/" OOPS_APP_ID, "/app0"};
    static char arg0[BUG_PATH_MAX];
    const char *root = nullptr;

    char *argv[] = {arg0, nullptr};

    (void)args;

    oops_log_init(OOPS_APP_ID);

    /* The kernel log drops bursts and this start-up is one, Pomme reading every model
       and sound resource before the first frame. The sink falls back to /data/<app id>,
       which has to exist when no USB stick is present. */
    (void)oops_fs_mkdir("/data/" OOPS_APP_ID, 0755);
    (void)oops_log_enable_disk_sink(OOPS_APP_ID, 0);

    oops_log_info("BUGD", "entry");

    /* Namespace-scope constructors: .init_array is not walked for this payload, so
       nothing else runs them. Idempotent, per oops/system.h. */
    oops_run_init_array();

    /* Pomme's FindFolder (Files.cpp:186) reads XDG_CONFIG_HOME then HOME and returns
       fnfErr when both are unset, which upstream answers with an alert box. .config is
       the directory it appends, and it is called with kDontCreateFolder. */
    (void)oops_fs_mkdir(OOPS_POSIX_HOME, 0755);
    (void)oops_fs_mkdir(OOPS_POSIX_HOME "/.config", 0755);
    if (setenv("HOME", OOPS_POSIX_HOME, 1) != 0) {
        /* Not fatal: only saving fails, and the symptom does not name this. */
        oops_log_error("BUGD", "HOME could not be set; preferences will not persist");
    }

    for (size_t i = 0; i < sizeof(roots) / sizeof(roots[0]); i++) {
        if (bug_root_has_data(roots[i])) {
            root = roots[i];
            break;
        }
    }
    if (root == nullptr) {
        /* Refused rather than started: upstream's remaining attempts end in a message
           box that names no path and needs a controller to dismiss. */
        oops_log_error("BUGD",
                       "no Data/Skeletons/DoodleBug.3dmf under %s or %s - "
                       "the assets are not deployed",
                       roots[0], roots[1]);
        return 1;
    }

    /* FindGameData takes parent_path, so the trailing name only has to be a name. */
    {
        const size_t n = bug_append(arg0, sizeof(arg0), 0u, root);
        if (n == 0u || bug_append(arg0, sizeof(arg0), n, "/Bugdom") == 0u) {
            oops_log_error("BUGD", "root path too long: %s", root);
            return 1;
        }
    }

    oops_log_info("BUGD", "argv[0]=%s HOME=%s", arg0, OOPS_POSIX_HOME);

    return main(1, argv);
}
