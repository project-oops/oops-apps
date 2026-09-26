#!/usr/bin/env sh
# Put every pinned dependency tree under `src/oops-deps/` on disk.
#
# Each dependency carries an `upstream.lock`, and `common/upstream-fetch.sh` fetches it into
# `upstream/` with `patches/` applied. `common/upstream.mk` fetches only a title's own lock, so
# this covers the dependencies. Every lock is fetched, since only a title's Makefile knows which
# it uses; a tree already at its revision costs a stamp comparison.
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
