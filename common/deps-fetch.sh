#!/usr/bin/env sh
# Put every pinned dependency tree under `src/oops-deps/` on disk.
#
# A dependency is vendored the same way a title's upstream is: an `upstream.lock` naming a kind,
# a URL and a revision, an `upstream/` directory it lands in, and a `patches/` directory applied
# on top. `common/upstream-fetch.sh` does one of them and is idempotent - it records what it
# fetched in a stamp and exits 0 when the stamp already matches - so this is a loop over the
# locks and nothing more.
#
# **Why it exists.** `common/upstream.mk` fetches a *title's* own upstream at Makefile-parse
# time, and only for a lock in the app's own directory. Dependencies are not apps, so nothing
# fetched theirs: four of them carry a hand-run `make <dep>-upstream` target and five carry no
# way at all. On a developer's machine the trees are already there from the day they were first
# needed, and the gap is invisible.
#
# It was not invisible in CI, it was just silent. Neverball's build asked for
# `sdl2-ttf/upstream/SDL_ttf.c`, make answered "No rule to make target", and the release
# workflow reported a title with nothing to ship - so a title with eight vendored dependencies
# had never once been built there, under four days of green ticks.
#
# Fetching every lock rather than only the ones an app names is deliberate: which dependencies a
# title uses is known to its Makefile and not to a shell script, and a clone that is already at
# its revision costs a stamp comparison. Being right without being told is worth more here than
# being minimal.
set -eu

HERE="$(cd "$(dirname "$0")" && pwd)"
APPS="$(cd "$HERE/.." && pwd)"
FETCH="$HERE/upstream-fetch.sh"
DEPS="$APPS/src/oops-deps"

[ -d "$DEPS" ] || { echo "deps-fetch: no $DEPS" >&2; exit 1; }

fetched=0
failed=0

for lock in "$DEPS"/*/upstream.lock; do
    [ -f "$lock" ] || continue
    dir="$(dirname "$lock")"
    name="$(basename "$dir")"

    kind="$(sed -n 's/^UPSTREAM_KIND=//p' "$lock")"
    url="$(sed -n 's/^UPSTREAM_URL=//p' "$lock")"
    rev="$(sed -n 's/^UPSTREAM_REV=//p' "$lock")"
    sparse="$(sed -n 's/^UPSTREAM_SPARSE=//p' "$lock")"

    if [ -z "$kind" ] || [ -z "$url" ] || [ -z "$rev" ]; then
        echo "deps-fetch: $name: lock is missing KIND, URL or REV" >&2
        failed=$((failed + 1))
        continue
    fi

    if UPSTREAM_SPARSE="$sparse" "$FETCH" "$kind" "$url" "$rev" "$dir/upstream" "$dir/patches"; then
        fetched=$((fetched + 1))
    else
        echo "deps-fetch: $name: fetch failed" >&2
        failed=$((failed + 1))
    fi
done

echo "deps-fetch: $fetched dependency tree(s) present"
if [ "$failed" -gt 0 ]; then
    echo "deps-fetch: $failed failed - see above" >&2
    exit 1
fi
