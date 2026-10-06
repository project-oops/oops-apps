/*
 * The console entry point: builds the argument vector a shell would give dEQP and runs
 * the loop in `shim/gl_cts_main.cpp`.
 *
 * Arguments come from `/app0/cts-args.txt`, one per line, so the case subset is a
 * run-time choice, never compiled in. With no file it runs `KHR-GL30.info.*`.
 *
 * A big-app cannot exit; it logs a last line and idles.
 */
#include <stddef.h>
#include <stdio.h>

#include "oops/system.h"
#include "oops/time.h"

/* `shim/gl_cts_main.cpp`: dEQP's run loop, plus `/app0/cts-status.txt`. */
int oops_cts_run_main(int argc, char **argv);

void gl_cts_start(void);

/* Room for a `--deqp-case` glob and a few switches; a longer command line is logged and
   cut. */
#define CTS_MAX_ARGS 32
#define CTS_ARG_BYTES 4096

static char s_argbuf[CTS_ARG_BYTES];
static char *s_argv[CTS_MAX_ARGS];
static char *s_file_argv[CTS_MAX_ARGS];

/*
 * Reads `/app0/cts-args.txt` into `s_file_argv`, one argument per line, and returns the
 * count; 0 if the file is absent. Blank lines and `#` lines are skipped. Called before
 * anything touches `/data`, which makes `/app0` unreachable.
 */
static int read_args_file(int start) {
    FILE *f = fopen("/app0/cts-args.txt", "r");
    if (f == NULL)
        return start;

    size_t used = 0;
    int argc = start;

    while (argc < CTS_MAX_ARGS) {
        char line[512];
        if (fgets(line, (int)sizeof line, f) == NULL)
            break;

        size_t n = 0;
        while (line[n] != '\0' && line[n] != '\n' && line[n] != '\r')
            n++;
        line[n] = '\0';

        if (n == 0 || line[0] == '#')
            continue;

        if (used + n + 1u > sizeof s_argbuf) {
            oops_log("gl-cts: /app0/cts-args.txt is longer than %u bytes; the rest is "
                     "ignored",
                     (unsigned)sizeof s_argbuf);
            break;
        }

        char *dst = &s_argbuf[used];
        for (size_t i = 0; i <= n; i++)
            dst[i] = line[i];
        used += n + 1u;

        s_file_argv[argc++] = dst;
    }

    fclose(f);

    if (argc == CTS_MAX_ARGS)
        oops_log("gl-cts: more than %d arguments; the rest is ignored",
                 CTS_MAX_ARGS - 1);

    return argc;
}

/*
 * The result log path. dEQP's default is relative and a title has no working directory.
 * `/app0` is writable and is the title's own directory, so the log lands beside the
 * test resources and the host reads it from `/data/homebrew/<id>`. Reaching `/data`
 * instead would need the sandbox escape, which unmounts `/app0` and every asset under
 * it.
 */
#define CTS_LOG_ARG "--deqp-log-filename=/app0/TestResults.qpa"

/*
 * Runs `.init_array`; a title has no crt, so nothing else does (oops-apps#D006). ACO's
 * opcode table is built by a constructor, and so is the CTS package registry
 * (`glcTestPackageEntry.cpp`); without this the suite reports zero cases.
 */
extern void oops_mesa_run_init_array(void);

void gl_cts_start(void) {
    oops_log("gl-cts: start");

    oops_mesa_run_init_array();

    s_argv[0] = (char *)"glcts";

    const int n_file_args = read_args_file(0);

    int first = 1;

    s_argv[first++] = (char *)CTS_LOG_ARG;

    /*
     * Where tests read shaders and reference images; dEQP's default `"."`
     * (`tcuCommandLine.cpp:272`) is relative. dEQP refuses a repeated option, so
     * `cts-args.txt` must not name this or the log filename.
     */
    s_argv[first++] = (char *)"--deqp-archive-dir=/app0";

    int argc = first;
    for (int i = 0; i < n_file_args && argc < CTS_MAX_ARGS; i++)
        s_argv[argc++] = s_file_argv[i];

    if (n_file_args == 0) {
        /*
         * No argument file: `KHR-GL30.info`, the vendor, renderer, version, GLSL
         * version, extension and render-target cases. They exercise platform, context,
         * driver and `.qpa` end to end, and report what the driver is.
         */
        s_argv[argc++] = (char *)"--deqp-case=KHR-GL30.info.*";
        oops_log("gl-cts: no /app0/cts-args.txt; running KHR-GL30.info.* (6 cases)");
    } else {
        oops_log("gl-cts: %d arguments from /app0/cts-args.txt", n_file_args);
    }

    for (int i = 1; i < argc; i++)
        oops_log("gl-cts:   argv[%d] = %s", i, s_argv[i]);

    const int rc = oops_cts_run_main(argc, s_argv);

    oops_log("gl-cts: dEQP returned %d", rc);
    oops_log("gl-cts: done");

    for (;;) {
        oops_time_sleep_ms(1000);
    }
}
