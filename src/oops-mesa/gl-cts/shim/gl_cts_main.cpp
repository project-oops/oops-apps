/*
 * The run loop: upstream's `tcuMain.cpp` with one addition, a status file the harness
 * writes itself.
 *
 * The `.qpa` goes through libc `open`, which reports success on this console and can
 * store nothing, so a result read only from it is no result. `/app0/cts-status.txt`
 * goes through `oops_fs`, which writes through `SYS_open` and persists. It says
 * `starting` before the command line is parsed, `running` with the counts so far as
 * cases finish, and at the end `done` (or `error` with the reason) with the final
 * counts:
 *
 *     state=running
 *     executed=4
 *     passed=4
 *     failed=0
 *     not_supported=0
 *     warnings=0
 *     waived=0
 *     device_lost=0
 *     complete=0
 *     elapsed_ms=812
 *
 * Each write goes to a temporary name and is renamed over the last, so a host that
 * reads the file mid-run sees one whole state or the one before, never half of one.
 */
#include "tcuDefs.hpp"
#include "tcuCommandLine.hpp"
#include "tcuPlatform.hpp"
#include "tcuApp.hpp"
#include "tcuResource.hpp"
#include "tcuTestLog.hpp"
#include "tcuTestPackage.hpp"
#include "deUniquePtr.hpp"
#include "qpDebugOut.h"

#include <cstdio>
#include <exception>

extern "C" {
#include "oops/fs.h"
#include "oops/system.h"
#include "oops/time.h"
}

/* `shim/tcuOopsPlatform.cpp`. */
tcu::Platform *createPlatform(void);

namespace {

const char kStatusPath[] = "/app0/cts-status.txt";
const char kStatusTemp[] = "/app0/cts-status.txt.tmp";

/* Between progress writes. A case can take microseconds, and a file write per case
 * would cost more than the cases do on a long subset; the final state is always
 * written. */
const uint64_t kProgressIntervalMs = 1000;

uint64_t s_startMs;

void writeStatus(const char *state, const tcu::TestRunStatus &s, const char *detail) {
    char buf[512];
    int len = snprintf(
        buf, sizeof buf,
        "state=%s\nexecuted=%d\npassed=%d\nfailed=%d\nnot_supported=%d\n"
        "warnings=%d\nwaived=%d\ndevice_lost=%d\ncomplete=%d\nelapsed_ms=%llu\n",
        state, s.numExecuted, s.numPassed, s.numFailed, s.numNotSupported,
        s.numWarnings, s.numWaived, s.numDeviceLost, s.isComplete ? 1 : 0,
        (unsigned long long)(oops_time_get_ms() - s_startMs));
    if (len < 0)
        return;
    if (detail != nullptr && (size_t)len < sizeof buf)
        len += snprintf(buf + len, sizeof buf - (size_t)len, "detail=%s\n", detail);
    if ((size_t)len >= sizeof buf)
        len = (int)sizeof buf - 1;

    if (oops_fs_write_all(kStatusTemp, buf, (size_t)len) != 0 ||
        oops_fs_rename(kStatusTemp, kStatusPath) != 0)
        oops_log("gl-cts: could not write %s (state=%s)", kStatusPath, state);
}

bool disableRawWrites(int, const char *) {
    return false;
}
bool disableFmtWrites(int, const char *, va_list) {
    return false;
}

} // namespace

extern "C" int oops_cts_run_main(int argc, char **argv) {
    int exitStatus = EXIT_SUCCESS;

    s_startMs = oops_time_get_ms();
    writeStatus("starting", tcu::TestRunStatus(), nullptr);

    setvbuf(stdout, nullptr, _IOLBF, 4 * 1024);

    try {
        tcu::CommandLine cmdLine(argc, argv);

        if (cmdLine.quietMode())
            qpRedirectOut(disableRawWrites, disableFmtWrites);

        tcu::DirArchive archive(cmdLine.getArchiveDir());
        tcu::TestLog log(cmdLine.getLogFileName(), cmdLine.getLogFlags());
        de::UniquePtr<tcu::Platform> platform(createPlatform());
        de::UniquePtr<tcu::App> app(new tcu::App(*platform, archive, log, cmdLine));

        int lastExecuted = -1;
        uint64_t lastWriteMs = 0;

        for (;;) {
            const bool more = app->iterate();
            const tcu::TestRunStatus &result = app->getResult();

            if (!more) {
                if (cmdLine.getRunMode() == tcu::RUNMODE_EXECUTE &&
                    (!result.isComplete || result.numFailed))
                    exitStatus = EXIT_FAILURE;
                writeStatus("done", result, nullptr);
                oops_log("gl-cts: %d executed, %d passed, %d failed, %d not supported, "
                         "%d warnings, %d waived%s",
                         result.numExecuted, result.numPassed, result.numFailed,
                         result.numNotSupported, result.numWarnings, result.numWaived,
                         result.isComplete ? "" : " - run incomplete");
                break;
            }

            if (result.numExecuted == lastExecuted)
                continue;
            const uint64_t now = oops_time_get_ms();
            if (now - lastWriteMs >= kProgressIntervalMs) {
                writeStatus("running", result, nullptr);
                lastExecuted = result.numExecuted;
                lastWriteMs = now;
            }
        }
    } catch (const std::exception &e) {
        writeStatus("error", tcu::TestRunStatus(), e.what());
        tcu::die("%s", e.what());
    }

    return exitStatus;
}
