/*
 * The console entry point, and the argument vector dEQP would otherwise take from a shell.
 *
 * `tcuMain.cpp` is an ordinary `int main(int argc, char **argv)` and is upstream's, unmodified.
 * A title here does not get one: the platform calls the entry named in the link, there is no
 * shell, and nothing hands it a command line. So this is the adapter, and it is deliberately the
 * whole of the adapter.
 *
 * # Where the arguments come from
 *
 * A file, `/app0/cts-args.txt`, one argument per line. **Not a compiled-in list**, and that is
 * the important part: which cases run has to be a run-time choice, because a suite whose subset
 * was fixed at build time is one this project curated rather than one it ran. `oops-mesa`'s
 * roadmap row 8 is explicit about it, and a compiled-in `--deqp-case` would quietly undo it.
 *
 * With no file, the defaults below run the smallest thing that proves the harness: the case
 * list is written out and nothing is executed. That is a deliberate choice of default - a first
 * run on new hardware should say what it *would* do before it does it.
 *
 * # Why it parks instead of returning
 *
 * A `big-app` container cannot terminate itself - obSCEne `REQ-20260917T1450Z-2e71` settled that
 * lifecycle belongs to the shell, `_exit` raises `SIGSYS`, and returning transfers to zero for
 * want of a caller frame. Every other Mesa title here ends the same way.
 */
#include <stddef.h>
#include <stdio.h>

#include "oops/system.h"
#include "oops/time.h"

/*
 * Not `main`. Under `-ffreestanding` - which `common/cxx.mk` uses - C++ stops special-casing
 * `int main(int, char **)`, so upstream's is in the archive as `_Z4mainiPPc` and this C file
 * cannot name it. `shim/tcuOopsPlatform.cpp` is compiled as C++ beside it and exports this
 * wrapper, which can.
 */
int oops_cts_run_main(int argc, char **argv);

void gl_cts_start(void);

/* Room for a command line. Sized for a `--deqp-case` glob plus the usual half-dozen switches;
   a longer one is refused loudly below rather than silently cut. */
#define CTS_MAX_ARGS 32
#define CTS_ARG_BYTES 4096

static char  s_argbuf[CTS_ARG_BYTES];
static char *s_argv[CTS_MAX_ARGS];

/*
 * Reads `/app0/cts-args.txt` into `s_argv`, one argument per line, and returns the count.
 * Returns 0 if the file is absent, which is not an error - it is the ordinary first run.
 *
 * Blank lines and lines beginning `#` are skipped, so the file can carry a note about why a
 * particular subset was chosen. That note is worth having beside the result.
 */
static int read_args_file(void)
{
    FILE *f = fopen("/app0/cts-args.txt", "r");
    if (f == NULL)
        return 0;

    size_t used = 0;
    int    argc = 1; /* argv[0] is filled by the caller */

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
            oops_log("gl-cts: /app0/cts-args.txt is longer than %u bytes; the rest is ignored",
                     (unsigned)sizeof s_argbuf);
            break;
        }

        char *dst = &s_argbuf[used];
        for (size_t i = 0; i <= n; i++)
            dst[i] = line[i];
        used += n + 1u;

        s_argv[argc++] = dst;
    }

    fclose(f);

    if (argc == CTS_MAX_ARGS)
        oops_log("gl-cts: more than %d arguments; the rest is ignored", CTS_MAX_ARGS - 1);

    return argc;
}

void gl_cts_start(void)
{
    oops_log("gl-cts: start");

    s_argv[0] = (char *)"glcts";

    int argc = read_args_file();
    if (argc == 0) {
        /*
         * No argument file. Write the case list and run nothing: the harness proves itself, the
         * platform is created, the GL context is made, and the result says what a real run would
         * cover - without spending an unknown amount of time on a first attempt.
         */
        s_argv[1] = (char *)"--deqp-runmode=stdout-caselist";
        argc      = 2;
        oops_log("gl-cts: no /app0/cts-args.txt; writing the case list and running nothing");
    } else {
        oops_log("gl-cts: %d arguments from /app0/cts-args.txt", argc - 1);
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
