#!/usr/bin/env bash
# upstream-fetch.sh - put a title's origin on disk at exactly the revision its lock names.
#
# One script for every title. It is called by `common/app.mk` and not by hand, and it knows
# nothing about any particular program: the lock file says where the source comes from and which
# revision, and this fetches that and applies whatever is in `patches/`.
#
#   upstream-fetch.sh <kind> <url> <rev> <dir> <patch-dir>
#
# **Why a lock file and a script rather than a submodule.** A title's origin as a submodule would
# be a third level - OOPS holds oops-apps, oops-apps would hold each origin - so every bump would
# cost three commits in three repositories, in a superproject whose log is already more than half
# bumps. And an asset-heavy origin wants `--depth 1 --filter=blob:none`, which `shallow = true`
# in `.gitmodules` asks for politely and this simply does. The reasoning is in
# `src/oops-titles/README.md`; a submodule remains available for an individual title if one
# suits it.
#
# **The fetched tree is never edited and never committed.** `.gitignore` drops `upstream/` the
# same way it drops `build/` and `dist/`: it is fetched output, not source. Our changes to the
# program live in `patches/`, and our code beside it in `shim/`.
set -euo pipefail

KIND="${1:?kind}"
URL="${2:?url}"
REV="${3:?rev}"
DIR="${4:?dir}"
PATCH_DIR="${5:-}"

STAMP="$DIR/.oops-upstream-stamp"

# **A sparse origin takes only the directories it names.** `UPSTREAM_SPARSE` in the lock is a
# space-separated list of top-level paths, and a lock without one fetches the whole tree as
# before. It exists because llvm-project is the first origin here where the whole tree is not a
# reasonable thing to ask for: libc++ is a few directories inside a monorepo of many gigabytes,
# and `git sparse-checkout` turns that into about 130 MB.
#
# The pin is unaffected. A sparse checkout is the same commit with fewer paths present, and the
# `rev-parse HEAD` check below still compares against the hash the lock names - so this changes
# what lands, never which revision it is.
#
# It is read here, above the stamp, because the stamp has to include it. See below.
SPARSE="${UPSTREAM_SPARSE:-}"

# Already at this revision with these patches and these paths: nothing to do. The stamp records
# all three, so changing any of them fetches again and changing none does not.
#
# **`$SPARSE` is in the stamp because leaving it out made this script lie.** It recorded only the
# revision and the patch sum, so widening `UPSTREAM_SPARSE` - adding `libunwind` and `libc` to
# libc++'s lock on 2026-09-21 - matched the stamp, exited 0, printed nothing, and left the tree
# exactly as it was. The caller then failed on a missing header with no indication that the fetch
# it had just run had declined to do anything. A no-op that reports success is the failure
# CONVENTIONS section 3 is about, and it is worse in a fetch than almost anywhere else, because
# every later step is reasoning about a tree it believes is current.
patch_sum() {
    if [ -n "$PATCH_DIR" ] && [ -d "$PATCH_DIR" ]; then
        # Sorted, so the sum is the set of patches rather than the order a glob happened to
        # return them in.
        cat $(ls "$PATCH_DIR"/*.patch 2>/dev/null | sort) 2>/dev/null | cksum | cut -d' ' -f1
    else
        printf '0'
    fi
}
# Sorted for the same reason the patches are: the set is what matters, not the order somebody
# happened to type it in, and a reordered lock should not cost a re-fetch.
sparse_key() {
    if [ -n "$SPARSE" ]; then
        printf '%s\n' $SPARSE | sort | tr '\n' ','
    else
        printf 'all'
    fi
}
WANT="$REV $(patch_sum) $(sparse_key)"

if [ -f "$STAMP" ] && [ "$(cat "$STAMP")" = "$WANT" ]; then
    exit 0
fi

case "$KIND" in
git) ;;
*)
    echo "upstream-fetch: unknown kind '$KIND' - the lock's UPSTREAM_KIND must be one this" >&2
    echo "                script implements. git is the only one so far; add another here" >&2
    echo "                rather than in a title." >&2
    exit 1
    ;;
esac

# A revision that is not a full hash is not a pin: a tag can be moved and a branch always does.
# The lock records both, and this checks out the hash - `UPSTREAM_REF` is there to tell a reader
# which release it is, not to decide what gets fetched.
case "$REV" in
    [0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f]*) ;;
    *)
        echo "upstream-fetch: UPSTREAM_REV must be a full commit hash, not '$REV'" >&2
        exit 1
        ;;
esac

# Past the stamp check, so a fetch is definitely happening. `common/upstream.mk` used to print
# this from an `$(info)` on the make side, where it could only be guarded by the stamp
# *existing* - see the comment there. Printed from here it is guarded by the thing it actually
# announces.
echo "${UPSTREAM_NAME:-upstream}: fetching upstream at ${UPSTREAM_REF:-$REV}"
echo "upstream: $URL @ $REV"
rm -rf "$DIR"
mkdir -p "$DIR"

# `SPARSE` is set above the stamp, which is the only thing that reads it before this point.

# **Shallow first, blobless second.** Asking for one commit is the cheapest thing that can work
# and most servers allow it; a server that does not (`uploadpack.allowReachableSHA1InWant` off)
# fails here rather than silently fetching something else, and the fallback clones history
# without file contents and fills them in on checkout. Either way what lands is the revision the
# lock names.
if ! (
    cd "$DIR"
    git init -q
    git remote add origin "$URL"
    if [ -n "$SPARSE" ]; then
        git config core.sparseCheckout true
        git sparse-checkout set --no-cone $SPARSE
    fi
    git fetch -q --depth 1 --filter=blob:none origin "$REV" 2>/dev/null
    git checkout -q FETCH_HEAD
); then
    echo "upstream: single-revision fetch refused, cloning blobless" >&2
    rm -rf "$DIR"
    if [ -n "$SPARSE" ]; then
        git clone -q --filter=blob:none --sparse --no-checkout "$URL" "$DIR"
        ( cd "$DIR" && git sparse-checkout set --no-cone $SPARSE && git checkout -q "$REV" )
    else
        git clone -q --filter=blob:none --no-checkout "$URL" "$DIR"
        ( cd "$DIR" && git checkout -q "$REV" )
    fi
fi

GOT="$(cd "$DIR" && git rev-parse HEAD)"
if [ "$GOT" != "$REV" ]; then
    echo "upstream-fetch: asked for $REV and got $GOT" >&2
    exit 1
fi

# **Patches are applied to a clean checkout, in name order, and a failure stops the build.** A
# patch that no longer applies means upstream moved under it, and the answer is to rebase the
# patch or move what it did into `shim/` - never to carry on with half of it applied.
if [ -n "$PATCH_DIR" ] && [ -d "$PATCH_DIR" ]; then
    for p in $(ls "$PATCH_DIR"/*.patch 2>/dev/null | sort); do
        echo "upstream: applying $(basename "$p")"
        ( cd "$DIR" && git apply --whitespace=nowarn "$p" ) || {
            echo "upstream-fetch: $(basename "$p") does not apply to $REV" >&2
            exit 1
        }
    done
fi

printf '%s' "$WANT" > "$STAMP"
