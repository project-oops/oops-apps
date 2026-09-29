#!/usr/bin/env sh
# Watch a build that is already running, from outside it.
#
# A build here is a quarter of an hour of clang command lines, which is not progress - it is
# the same information a thousand times over, and none of it says how far through it is. So
# this counts what exists rather than what was printed: objects on disk against the sources
# the app compiles, refreshed in place.
#
# It reads nothing but the filesystem and starts nothing, so it is safe to run against a
# build driven from somewhere else - another terminal, CI, or an agent's container.
#
# Usage:  common/build-progress.sh [app-dir]     (default: the current directory)
set -u

DIR="${1:-.}"
[ -d "$DIR" ] || { echo "build-progress: no directory '$DIR'" >&2; exit 1; }
cd "$DIR" || exit 1
NAME="$(basename "$PWD")"

# The denominator: every source the app could compile. An app that excludes some of them
# finishes a little early, which is a better error than one that never reaches 100%.
total=$(find upstream src shim -name '*.cpp' -o -name '*.cc' -o -name '*.c' 2>/dev/null | wc -l)
[ "$total" -gt 0 ] || total=1

start=$(date +%s)
last=0

while :; do
    done_now=$(find build -name '*.o' 2>/dev/null | wc -l)
    now=$(date +%s)
    elapsed=$((now - start))

    pct=$((done_now * 100 / total))
    [ "$pct" -gt 100 ] && pct=100

    # Rate over this run of the watcher, so an ETA appears once there is something to
    # measure and says nothing before then.
    eta="--:--"
    if [ "$elapsed" -gt 10 ] && [ "$done_now" -gt "$last" ] && [ "$done_now" -lt "$total" ]; then
        rate=$(( (done_now - last) * 60 / elapsed ))
        if [ "$rate" -gt 0 ]; then
            secs=$(( (total - done_now) * 60 / rate ))
            eta=$(printf '%02d:%02d' $((secs / 60)) $((secs % 60)))
        fi
    fi
    [ "$last" -eq 0 ] && last=$done_now

    bar=""
    filled=$((pct / 4))
    i=0
    while [ "$i" -lt 25 ]; do
        if [ "$i" -lt "$filled" ]; then bar="$bar#"; else bar="$bar."; fi
        i=$((i + 1))
    done

    printf '\r%s [%s] %3d%%  %d/%d objects  %02d:%02d elapsed  eta %s  ' \
        "$NAME" "$bar" "$pct" "$done_now" "$total" \
        $((elapsed / 60)) $((elapsed % 60)) "$eta"

    # The link is the last thing to appear and the one step no object count predicts.
    if [ -n "$(find build -maxdepth 1 -name '*.elf' -newermt "-20 seconds" 2>/dev/null)" ]; then
        printf '\n%s: linked\n' "$NAME"
        exit 0
    fi

    sleep 2
done
