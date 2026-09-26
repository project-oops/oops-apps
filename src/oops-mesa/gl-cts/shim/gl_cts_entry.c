/*
 * The console entry point: builds the argument vector a shell would give upstream's
 * unmodified `tcuMain.cpp` and runs it.
 *
 * Arguments come from `/app0/cts-args.txt`, one per line, so the case subset is a
 * run-time choice, never compiled in. With no file it runs `KHR-GL30.info.*`.
 *
 * A big-app cannot exit; it logs a last line and idles.
 */
#include <errno.h>
#include <fcntl.h> /* the raw open/write arm of the write probe */
#include <stddef.h>
#include <stdio.h>
#include <unistd.h> /* write, fsync, read, close - same */

#include "oops/fs.h"
#include "oops/system.h"
#include "oops/time.h"

/*
 * Under `-ffreestanding` upstream's `main` is mangled (`_Z4mainiPPc`), so
 * `shim/tcuOopsPlatform.cpp` exports this C wrapper for it.
 */
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
 * Is `/app0` still reachable? `oops_fs_storage_path` raises sandbox-escape privileges
 * (`oops/fs.h`), which repoints the process root, and `/app0` then names nothing. That
 * loses `cts-args.txt`, the archive dir, and the `/app0/eboot.bin` descriptor oops-mesa
 * opens for the GPU device (`src/winsys/drm_device.c`), without which there is no GL
 * context. `eboot.bin` is always present and is the file oops-mesa opens.
 */
static int app0_is_reachable(const char *when) {
    FILE *f = fopen("/app0/eboot.bin", "rb");

    if (f == NULL) {
        oops_log("gl-cts: /app0 is NOT reachable %s (errno %d) - "
                 "oops-mesa cannot open its device descriptor and there will be no GL "
                 "context",
                 when, errno);
        return 0;
    }

    fclose(f);
    oops_log("gl-cts: /app0 reachable %s", when);
    return 1;
}

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
 * The result log path. dEQP's default is relative and a title has no working directory;
 * writing under `/data` needs the sandbox-escape privilege that `oops_fs_storage_path`
 * raises while resolving the path (`oops/fs.h`), so the path comes from it.
 */
static char s_log_arg[320];

static const char *resolve_log_argument(void) {
    char path[256];

    if (oops_fs_storage_path(OOPS_STORAGE_APP_DATA, "TestResults.qpa", path,
                             sizeof path) != 0) {
        oops_log("gl-cts: oops_fs_storage_path refused; dEQP will have nowhere to "
                 "write its log");
        return NULL;
    }

    (void)snprintf(s_log_arg, sizeof s_log_arg, "--deqp-log-filename=%s", path);
    return s_log_arg;
}

/*
 * Runs `.init_array`; a title has no crt, so nothing else does (oops-apps#D006). ACO's
 * opcode table is built by a constructor, and so is the CTS package registry
 * (`glcTestPackageEntry.cpp`); without this the suite reports zero cases.
 */
extern void oops_mesa_run_init_array(void);

/*
 * Writes a known string to app storage three ways - stdio, raw `open`/`write`/`fsync`,
 * and `oops_fs_write_all` - and reads each back in this process, logging every return
 * value and errno. dEQP writes its `.qpa` through stdio (`qpTestLog.c`), so this
 * separates discarded writes, an unflushed buffer, and a process that sees a different
 * file from the host.
 */
static void report_file_write(void) {
    char path[256];

    if (oops_fs_storage_path(OOPS_STORAGE_APP_DATA, "writetest.txt", path,
                             sizeof path) != 0) {
        oops_log("gl-cts: writetest - storage path refused");
        return;
    }

    static const char payload[] = "oops-gl-cts write probe\n";
    const size_t want = sizeof payload - 1u;

    errno = 0;
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        oops_log("gl-cts: writetest - fopen(%s) failed, errno %d", path, errno);
        return;
    }

    errno = 0;
    const size_t wr = fwrite(payload, 1, want, f);
    const int ew = errno;

    errno = 0;
    const int fl = fflush(f);
    const int efl = errno;

    errno = 0;
    const int cl = fclose(f);
    const int ecl = errno;

    oops_log("gl-cts: writetest %s: fwrite %u/%u errno %d, fflush %d errno %d, fclose "
             "%d errno %d",
             path, (unsigned)wr, (unsigned)want, ew, fl, efl, cl, ecl);

    /* Read back in this process: a broken libc, not a namespace split, fails here. */
    errno = 0;
    FILE *rf = fopen(path, "rb");
    if (rf == NULL) {
        oops_log("gl-cts: writetest - reopen failed, errno %d (the bytes did not land)",
                 errno);
        return;
    }

    char buf[64];
    const size_t rd = fread(buf, 1, sizeof buf - 1u, rf);
    fclose(rf);
    buf[rd] = '\0';

    if (rd == want)
        oops_log("gl-cts: writetest - read back %u bytes, so this process can write "
                 "and read a "
                 "file; if the host sees 0 they are not the same file",
                 (unsigned)rd);
    else
        oops_log("gl-cts: writetest - read back %u bytes, expected %u", (unsigned)rd,
                 (unsigned)want);

    /*
     * The same with no stdio: separates the FILE layer never issuing `write(2)` from
     * `write(2)` reporting a count it did not deliver. `fsync` shows a write that never
     * reaches storage.
     */
    char rawpath[256];

    if (oops_fs_storage_path(OOPS_STORAGE_APP_DATA, "writeraw.txt", rawpath,
                             sizeof rawpath) != 0) {
        oops_log("gl-cts: writeraw - storage path refused");
        return;
    }

    errno = 0;
    const int fd = open(rawpath, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        oops_log("gl-cts: writeraw - open failed, errno %d", errno);
        return;
    }

    errno = 0;
    const ssize_t wrote = write(fd, payload, want);
    const int ewr = errno;

    errno = 0;
    const int synced = fsync(fd);
    const int esy = errno;

    errno = 0;
    const int closed = close(fd);
    const int ecl2 = errno;

    oops_log("gl-cts: writeraw %s: write %d/%u errno %d, fsync %d errno %d, close %d "
             "errno %d",
             rawpath, (int)wrote, (unsigned)want, ewr, synced, esy, closed, ecl2);

    errno = 0;
    const int rfd = open(rawpath, O_RDONLY);
    if (rfd < 0) {
        oops_log("gl-cts: writeraw - reopen failed, errno %d", errno);
        return;
    }

    char rbuf[64];
    const ssize_t got = read(rfd, rbuf, sizeof rbuf);
    close(rfd);

    oops_log("gl-cts: writeraw - read back %d bytes, expected %u%s", (int)got,
             (unsigned)want,
             (got == (ssize_t)want)
                 ? " - so the syscalls work and stdio is the problem"
                 : " - so write(2) itself reports what it did not do");

    /*
     * The same through the SDK's supported file layer, `oops_fs_write_all`, which is
     * `write(2)` with oops-sdk's open flags and retry loop
     * (`oops-sdk/src/system/fs.c`).
     */
    char sdkpath[256];

    if (oops_fs_storage_path(OOPS_STORAGE_APP_DATA, "writesdk.txt", sdkpath,
                             sizeof sdkpath) != 0) {
        oops_log("gl-cts: writesdk - storage path refused");
        return;
    }

    const int wrc = oops_fs_write_all(sdkpath, payload, want);

    void *back = NULL;
    size_t bsz = 0;
    const int rrc = oops_fs_read_all(sdkpath, &back, &bsz);
    if (back != NULL)
        oops_fs_free_data(back);

    oops_log("gl-cts: writesdk %s: oops_fs_write_all %d, oops_fs_read_all %d, read "
             "back %u/%u",
             sdkpath, wrc, rrc, (unsigned)bsz, (unsigned)want);
}

void gl_cts_start(void) {
    oops_log("gl-cts: start");

    oops_mesa_run_init_array();

    s_argv[0] = (char *)"glcts";

    /* Everything that reads `/app0` runs first: `resolve_log_argument()` raises the
     * sandbox escape, after which `/app0` is unreachable. */
    const int app0_before = app0_is_reachable("before the log path is resolved");
    const int n_file_args = read_args_file(0);

    const char *log_arg = resolve_log_argument();

    /* The same check after the escape; a change explains a missing GL context. */
    const int app0_after = app0_is_reachable("after the log path is resolved");

    if (app0_before && !app0_after)
        oops_log(
            "gl-cts: the sandbox escape in oops_fs_storage_path took /app0 with it - "
            "resources and the device descriptor are both below it");

    int first = 1;

    if (log_arg != NULL)
        s_argv[first++] = (char *)log_arg;

    /*
     * Where tests read shaders and reference images; dEQP's default `"."`
     * (`tcuCommandLine.cpp:272`) is relative. `/app0` while it is reachable, otherwise
     * the same directory by its real path under `/data/homebrew`. dEQP refuses a
     * repeated option, so `cts-args.txt` must not name this or the log filename.
     */
    s_argv[first++] = app0_after
                          ? (char *)"--deqp-archive-dir=/app0"
                          : (char *)"--deqp-archive-dir=/data/homebrew/" OOPS_APP_ID;

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

    report_file_write();

    oops_log("gl-cts: done");

    for (;;) {
        oops_time_sleep_ms(1000);
    }
}
