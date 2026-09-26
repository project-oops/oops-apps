/*
 * The console entry point, and the argument vector dEQP would otherwise take from a
 * shell.
 *
 * `tcuMain.cpp` is an ordinary `int main(int argc, char **argv)` and is upstream's,
 * unmodified. A title here does not get one: the platform calls the entry named in the
 * link, there is no shell, and nothing hands it a command line. So this is the adapter,
 * and it is deliberately the whole of the adapter.
 *
 * # Where the arguments come from
 *
 * A file, `/app0/cts-args.txt`, one argument per line. **Not a compiled-in list**, and
 * that is the important part: which cases run has to be a run-time choice, because a
 * suite whose subset was fixed at build time is one this project curated rather than
 * one it ran. `oops-mesa`'s roadmap row 8 is explicit about it, and a compiled-in
 * `--deqp-case` would quietly undo it.
 *
 * With no file, the defaults below run the smallest thing that proves the harness: the
 * case list is written out and nothing is executed. That is a deliberate choice of
 * default - a first run on new hardware should say what it *would* do before it does
 * it.
 *
 * # Why it parks instead of returning
 *
 * A `big-app` container cannot terminate itself - obSCEne `REQ-20260917T1450Z-2e71`
 * settled that lifecycle belongs to the shell, `_exit` raises `SIGSYS`, and returning
 * transfers to zero for want of a caller frame. Every other Mesa title here ends the
 * same way.
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
 * Not `main`. Under `-ffreestanding` - which `common/cxx.mk` uses - C++ stops
 * special-casing `int main(int, char **)`, so upstream's is in the archive as
 * `_Z4mainiPPc` and this C file cannot name it. `shim/tcuOopsPlatform.cpp` is compiled
 * as C++ beside it and exports this wrapper, which can.
 */
int oops_cts_run_main(int argc, char **argv);

void gl_cts_start(void);

/* Room for a command line. Sized for a `--deqp-case` glob plus the usual half-dozen
   switches; a longer one is refused loudly below rather than silently cut. */
#define CTS_MAX_ARGS 32
#define CTS_ARG_BYTES 4096

static char s_argbuf[CTS_ARG_BYTES];
static char *s_argv[CTS_MAX_ARGS];
static char *s_file_argv[CTS_MAX_ARGS];

/*
 * Is `/app0` still reachable?
 *
 * **This is not a debugging leftover, it is the arm for the fault that cost the first
 * run with test packages in it.** `/app0` is a jail-relative mount, and
 * `oops_fs_storage_path` raises sandbox-escape privileges to reach `/data` -
 * `oops/fs.h:88` says so plainly. The escape repoints the process's root:
 *
 *     [SANDBOX] repointed fd_rdir, fd_jdir, and fd_cdir to rootvnode (...) for PID 3783
 *
 * and from that moment `/app0` names nothing. Everything that reads it afterwards
 * fails, and none of them say why:
 *
 *   - `/app0/cts-args.txt` is reported "not present", which is indistinguishable from a
 * run that simply has no argument file. The run-time subset mechanism was silently
 * dead.
 *   - `--deqp-archive-dir=/app0` sends every test resource lookup to a path that is
 * gone.
 *   - **oops-mesa opens `/app0/eboot.bin`** to get a real, dup-able descriptor for the
 * GPU device (`src/winsys/drm_device.c:718`; it needs a dup-able fd because the Gallium
 * DRI frontend dups it). That open returned ENOENT, oops-mesa fell back to the `0x57`
 * token, and the token cannot be dupped - so `driCreateNewScreen3` returned NULL and
 * there was no GL context at all.
 *
 * The visible symptom was three layers away from the cause, in somebody else's project,
 * and read exactly like a Mesa gap. So the reachability is measured on both sides of
 * the call that changes it and said out loud, which turns a one-line log into the whole
 * diagnosis.
 *
 * `eboot.bin` is the file to test because it is the one file `/app0` is guaranteed to
 * hold - it is what is executing - and because it is the one oops-mesa itself opens.
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
 * count. Returns 0 if the file is absent, which is not an error - it is the ordinary
 * first run.
 *
 * Blank lines and lines beginning `#` are skipped, so the file can carry a note about
 * why a particular subset was chosen. That note is worth having beside the result.
 *
 * **It fills its own array rather than `s_argv` directly, and it is called before
 * anything touches `/data`.** Both are the same fix: this reads from `/app0`, and the
 * log path is resolved through a call that makes `/app0` unreachable, so the order is
 * not a preference. It used to be called second, and the file it could no longer open
 * was reported as an absent file - the one report that looks exactly like the ordinary
 * case.
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
 * Where the result log goes, and why it is not a string constant.
 *
 * dEQP defaults to `TestResults.qpa` - a *relative* path resolved against a working
 * directory a title here does not have - and treats failing to open it as fatal,
 * correctly: the log is not a side effect of a run, it *is* the run's output. The first
 * hardware run got as far as building the platform and stopped there.
 *
 * **An absolute `/data/homebrew/<id>/` path is not enough either**, and that was the
 * second attempt. `oops/fs.h:88` says why: *"On target hardware, accessing /data or
 * /mnt/usb automatically ensures sandbox escape privileges"* - the privilege is raised
 * **by the SDK call** that resolves the directory, not by naming the path. A title that
 * writes the path itself is refused.
 *
 * So the path comes from `oops_fs_storage_path`, which resolves it, ensures the
 * directory exists, and takes care of the privilege on the way.
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
 * Runs `.init_array`. Nothing else does: a title here has no crt, so namespace-scope
 * constructors never run unless somebody calls this (oops-mesa worklog 061-062, and
 * `oops-apps#D006` records the same thing biting `cxxrt.cpp`'s `set_terminate`).
 *
 * **Two separate things in this title depend on it, and both fail silently without
 * it.**
 *
 * ACO's opcode table is built by a constructor, and oops-mesa found it all zeroes on
 * hardware - an `ILLEGAL_INST` a long way from the cause.
 *
 * And `external/openglcts/modules/glcTestPackageEntry.cpp` is *nothing but* a
 * constructor:
 *
 *     RegisterCTSPackages g_registerCTS;
 *
 * which is how every CTS test package enters the registry. Without this call the suite
 * starts, opens its log, finds an empty hierarchy and reports zero cases - which looks
 * exactly like "the test modules are not built yet" and is not.
 */
extern void oops_mesa_run_init_array(void);

/*
 * Does writing a file work at all here?
 *
 * The suite ran 6/6 on hardware and `/data/GCTS00001/TestResults.qpa` came back **0
 * bytes**. The log opened - dEQP printed "Writing test log into ..." and the file
 * exists on the console with the right name and mode - and `qpTestLog.c` writes it with
 * plain `fopen`/`fprintf`/`fflush`/ `fclose` (lines 330, 375, 249, 413). `tcu::TestLog`
 * is a stack object inside `tcuMain.cpp`'s try block, so its destructor ran and closed
 * the file before `main` returned, which it did: "dEQP returned 0" is printed after.
 *
 * So the bytes went somewhere that is not that file, and **nothing in this title has
 * ever successfully written one**. The only other file operation is reading
 * `/app0/cts-args.txt`, which has returned NULL on every run because the sandbox escape
 * takes `/app0` away first - so that is not evidence of a working `fopen` either.
 *
 * Three candidates, and they need different fixes, which is why this measures instead
 * of guessing:
 *
 *   - writes are discarded (`fwrite` reports success, nothing lands)
 *   - writes are buffered and the flush does not reach the disk (`fflush`/`fclose` are
 * stubs)
 *   - the file this process writes is not the file the host sees (namespace, after the
 * escape)
 *
 * The first two are told apart by the return values below. The third is told apart by
 * the read-back: this writes a known string, closes, reopens and reads it in the *same*
 * process. If the read-back succeeds and the host still sees 0 bytes, the process and
 * the host are looking at different files, and the log path is the thing to change
 * rather than the libc.
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

    /* Read it back in this same process. Distinguishes a broken libc from a namespace
     * split. */
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
     * The same thing again with no stdio in the way.
     *
     * Measured 2026-09-25: the stdio arm above reports complete success at every step -
     * `fwrite` 24/24, `fflush` 0, `fclose` 0, errno never set - and reading the file
     * back *in this same process* gives 0 bytes. The file is created; the bytes are
     * not. Because the read-back is in the same process and at the same path, this is
     * not a namespace split.
     *
     * What is left is whether the FILE layer ever issues a `write(2)`, or whether
     * `write(2)` itself reports a count it did not deliver. Those are different repairs
     * - one is the C library's buffering, the other is the platform's file write after
     * a sandbox escape - and a raw `open`/`write`/`fsync` separates them in one run.
     * `fsync` is included because a successful `write` that never reaches storage would
     * show up only there.
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
     * And the same thing through the SDK's own file layer, which is the supported way
     * to write a file here and the one a finding should be stated in terms of.
     *
     * `oops_fs_write_all` is `write(2)` underneath (`oops-sdk/src/system/fs.c:108`), so
     * this is not a third mechanism - it is the same syscall reached through the API
     * that oops-sdk guarantees, with whatever open flags and retry loop it applies. If
     * this works and stdio does not, gl-cts has an immediate fix (route the `.qpa`
     * through it) and the finding is "dEQP uses stdio and stdio does not write here".
     * If this fails too, the write path itself is the finding and it belongs to
     * oops-sdk.
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

    /*
     * **Everything that reads `/app0` happens here, before anything touches `/data`.**
     * The order is a hard requirement, not a tidiness: `resolve_log_argument()` below
     * raises sandbox-escape privileges and that makes `/app0` unreachable for the rest
     * of the process. `app0_is_reachable()` above has the whole account.
     */
    const int app0_before = app0_is_reachable("before the log path is resolved");
    const int n_file_args = read_args_file(0);

    const char *log_arg = resolve_log_argument();

    /*
     * The same question again, on the other side of the call that changes the answer.
     * If these two disagree, the line below is the entire diagnosis of a GL context
     * that will not be created several layers further down, in another project.
     */
    const int app0_after = app0_is_reachable("after the log path is resolved");

    if (app0_before && !app0_after)
        oops_log(
            "gl-cts: the sandbox escape in oops_fs_storage_path took /app0 with it - "
            "resources and the device descriptor are both below it");

    int first = 1;

    if (log_arg != NULL)
        s_argv[first++] = (char *)log_arg;

    /*
     * Where the tests read their shaders and reference images from.
     *
     * `tcuCommandLine.cpp:272` defaults `--deqp-archive-dir` to `"."` - a *relative*
     * path, and the same trap the log filename sprang: a title here has no working
     * directory for it to be relative to. Unlike the log this one fails late and
     * quietly, in whichever test first opens a resource, as a `tcu::ResourceError` a
     * long way from the cause.
     *
     * **Which path depends on whether `/app0` survived.** `make stage-data` puts the
     * tree at
     * `/app0/gl_cts`, and that is the name to use while the title is still inside its
     * jail. Once the escape has happened `/app0` is gone and the same bytes are
     * reachable by their real path - the title directory under `/data/homebrew` -
     * because that is what the loader mounted as `/app0` in the first place.
     *
     * **Not overridable from `cts-args.txt`, and the comment here used to claim it
     * was.** dEQP refuses a repeated option rather than taking the last one: it prints
     * "Command line option
     * '--deqp-case' specified multiple times", dumps its usage and exits without
     * running a case. Measured on hardware 2026-09-25 by putting eight `--deqp-case`
     * lines in the file.
     *
     * So this argument and the log filename above are fixed for the run, and a
     * `cts-args.txt` that names either of them stops the run instead of changing it.
     * `cts-args.txt` says so.
     */
    s_argv[first++] = app0_after
                          ? (char *)"--deqp-archive-dir=/app0"
                          : (char *)"--deqp-archive-dir=/data/homebrew/" OOPS_APP_ID;

    int argc = first;
    for (int i = 0; i < n_file_args && argc < CTS_MAX_ARGS; i++)
        s_argv[argc++] = s_file_argv[i];

    if (n_file_args == 0) {
        /*
         * No argument file: run `KHR-GL30.info`. Six cases - vendor, renderer, version,
         * shading language version, the extension list and the render target - and they
         * are the smallest thing that exercises the whole chain end to end: the
         * platform is built, a GL context is created, the driver is asked real
         * questions and six real results reach the `.qpa`.
         *
         * **The default used to be `--deqp-runmode=stdout-caselist`**, which was right
         * while no test package registered: the list was empty and printing it cost
         * nothing. With twenty-five packages registered that is the entire OpenGL CTS
         * case list - hundreds of thousands of names - pushed one line at a time
         * through `oops_log` and the kernel log pipe, which is minutes of output nobody
         * reads to learn something `--deqp-runmode` can be asked for deliberately.
         *
         * It is also the wrong *kind* of default now. A case list says what the binary
         * contains, which the build already knows; `info` says what the *driver* is,
         * which nothing here knows until it runs, and which is the input to choosing
         * any real subset.
         *
         * `oops-mesa`'s roadmap row 8 is why this is a default rather than a
         * compile-time choice: which cases run has to be a run-time decision, and
         * `/app0/cts-args.txt` is where a real one is made.
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
