#!/usr/bin/env sh
# How many compiles to run at once, printed as make flags.
#
# Every build here went through `make` with no `-j` at all, so a 32-core machine compiled
# one file at a time and a full rebuild of a title that carries libultraship, ZAPD and
# OTRExporter - some 1,100 translation units - took a quarter of an hour for no reason.
#
# The number is not just the core count, because two things can make that wrong:
#
#   - **Memory.** A C++ compile of this codebase peaks near a gigabyte, so 32 at once on a
#     machine with 8GB free is an out-of-memory failure rather than a fast build. The count
#     is capped at one job per gigabyte of *available* memory, which is the figure that
#     accounts for what everything else is already holding.
#
#   - **Other work.** Sessions build here concurrently, and `-l` is how make is told to use
#     what is free rather than what exists: it starts no new job while the load average is
#     already at the core count, so two builds share the machine instead of each trying to
#     own it. That is the "when available" part, and it is why this is not simply `-j$(nproc)`.
#
# An explicit choice always wins: `OOPS_JOBS=4` sets the count, `OOPS_JOBS=0` turns
# parallelism off entirely, and a `-j` already in `MAKEFLAGS` is left alone.
#
# Usage:  make $(common/jobs.sh) title
set -u

# Already asked for? Then this has no opinion.
case " ${MAKEFLAGS:-} " in
    *" -j"* | *" --jobs"*) exit 0 ;;
esac
# GNU make also folds short flags together, so `-j` can arrive without its dash.
case "${MAKEFLAGS:-}" in
    [!-]*j*) exit 0 ;;
esac

if [ -n "${OOPS_JOBS:-}" ]; then
    case "$OOPS_JOBS" in
        0) exit 0 ;;
        *[!0-9]* | "")
            echo "jobs.sh: OOPS_JOBS must be a number, not '$OOPS_JOBS'" >&2
            exit 1
            ;;
        *) printf -- '-j%s' "$OOPS_JOBS"; exit 0 ;;
    esac
fi

cores=$(nproc 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)
jobs=$((cores * 2 / 3))
[ "$jobs" -lt 1 ] && jobs=1

# `MemAvailable` rather than `MemFree`: the kernel's own estimate of what can be had
# without swapping, which is what a compile actually needs. Absent (macOS, Git Bash), the
# core count stands on its own.
if [ -r /proc/meminfo ]; then
    avail_kb=$(awk '/^MemAvailable:/ { print $2; exit }' /proc/meminfo 2>/dev/null || echo "")
    if [ -n "$avail_kb" ]; then
        by_mem=$((avail_kb / 1024 / 1024))
        [ "$by_mem" -lt 1 ] && by_mem=1
        [ "$by_mem" -lt "$jobs" ] && jobs=$by_mem
    fi
fi

flags="-j$jobs"

# The load limit needs a load average to read, which not every host has.
if [ -r /proc/loadavg ]; then
    flags="$flags -l$cores"
fi

printf -- '%s' "$flags"
