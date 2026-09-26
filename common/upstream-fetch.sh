#!/usr/bin/env bash
# upstream-fetch.sh - put a title's origin on disk at exactly the revision its lock names.
#
#   upstream-fetch.sh <kind> <url> <rev> <dir> <patch-dir>
#
# Called by `common/upstream.mk`, not by hand. The lock says where the source comes from and at
# which revision; this fetches it and applies `patches/`. A lock rather than a submodule keeps
# each bump to one repository and allows a shallow, blobless fetch (`src/oops-titles/README.md`).
# The fetched tree is ignored by git and never edited; changes live in `patches/` and `shim/`.
set -euo pipefail

KIND="${1:?kind}"
URL="${2:?url}"
REV="${3:?rev}"
DIR="${4:?dir}"
PATCH_DIR="${5:-}"

STAMP="$DIR/.oops-upstream-stamp"

# `UPSTREAM_SPARSE`: space-separated top-level paths for a sparse checkout (llvm-project's
# libc++), or empty for the whole tree. The commit is the same either way.
SPARSE="${UPSTREAM_SPARSE:-}"

# `UPSTREAM_SUBMODULES=1`: check out the origin's submodules at the revisions its tree records.
SUBMODULES="${UPSTREAM_SUBMODULES:-}"

# The stamp records the revision, the patch set, the sparse paths and the submodule switch, so
# changing any of them fetches again and changing none does not.
patch_sum() {
    if [ -n "$PATCH_DIR" ] && [ -d "$PATCH_DIR" ]; then
        # Sorted, so the sum is of the set of patches, not the glob order.
        cat $(ls "$PATCH_DIR"/*.patch 2>/dev/null | sort) 2>/dev/null | cksum | cut -d' ' -f1
    else
        printf '0'
    fi
}
# Sorted, so a reordered lock does not re-fetch.
sparse_key() {
    if [ -n "$SPARSE" ]; then
        printf '%s\n' $SPARSE | sort | tr '\n' ','
    else
        printf 'all'
    fi
}
WANT="$REV $(patch_sum) $(sparse_key) sub=${SUBMODULES:-0}"

# Fails on any file with CRLF in the working tree and LF in the index, unless upstream's own
# `.gitattributes` asked for `eol=crlf`. A CRLF data file fails at run time on hardware, so
# this is fatal. It runs after `git apply`, which honours the repository's eol settings; the
# checkout goes through `$GIT` and cannot produce CRLF. `--eol` reads every file, so on the
# up-to-date path it runs only with `UPSTREAM_CHECK_EOL=1`.
check_eol() {
    crlf="$(cd "$DIR" && git ls-files --eol 2>/dev/null \
            | grep 'i/lf[[:space:]]*w/crlf' \
            | grep -vc 'eol=crlf' || true)"
    [ "${crlf:-0}" -gt 0 ] || return 0
    echo "upstream-fetch: $crlf files in $DIR have CRLF in the working tree and LF in the index," >&2
    echo "                and upstream's .gitattributes did not ask for it." >&2
    echo "                This tree is not the revision the lock names, and a program that reads its" >&2
    echo "                own data files will fail on the extra byte - Extreme Tux Racer lost its" >&2
    echo "                fonts, music and six textures to exactly this on 2026-09-24." >&2
    echo "                Run 'make upstream-clean' and build again; the fetch now checks out with" >&2
    echo "                core.autocrlf=false." >&2
    exit 1
}

if [ -f "$STAMP" ] && [ "$(cat "$STAMP")" = "$WANT" ]; then
    if [ -n "${UPSTREAM_CHECK_EOL:-}" ]; then
        check_eol
    fi
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

# Only a full hash is a pin; `UPSTREAM_REF` names the release for a reader and fetches nothing.
case "$REV" in
    [0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f]*) ;;
    *)
        echo "upstream-fetch: UPSTREAM_REV must be a full commit hash, not '$REV'" >&2
        exit 1
        ;;
esac

# Printed here, past the stamp check, because only now is a fetch certain.
echo "${UPSTREAM_NAME:-upstream}: fetching upstream at ${UPSTREAM_REF:-$REV}"
echo "upstream: $URL @ $REV"

# Without git, refuse before anything is deleted, so the existing tree survives. The
# `silkeh/clang` container carries no git.
if ! command -v git >/dev/null 2>&1; then
    echo "upstream-fetch: no git on PATH - refusing, so the existing checkout is left alone" >&2
    echo "upstream-fetch: run the fetch on the host; the clang container carries no git" >&2
    exit 1
fi
# Emptied before removal, since Windows often holds the directory itself open. `.[!.]*` takes
# `.git` and the stamp, so the next `git init` inherits nothing.
rm -rf "$DIR"/* "$DIR"/.[!.]* 2>/dev/null || true
rm -rf "$DIR" 2>/dev/null || true
mkdir -p "$DIR"

# Every git call that writes the tree goes through `$GIT`, so the tree lands byte for byte
# whatever the machine's `core.autocrlf`; `core.eol=lf` covers an origin with `text=auto`.
GIT="git -c core.autocrlf=false -c core.eol=lf"

# A single-revision shallow fetch first; a server that refuses it falls back to a blobless
# clone. Either way the checkout is the revision the lock names.
if ! (
    cd "$DIR"
    $GIT init -q
    git remote add origin "$URL"
    if [ -n "$SPARSE" ]; then
        git config core.sparseCheckout true
        git sparse-checkout set --no-cone $SPARSE
    fi
    $GIT fetch -q --depth 1 --filter=blob:none origin "$REV" 2>/dev/null
    $GIT checkout -q FETCH_HEAD
); then
    echo "upstream: single-revision fetch refused, cloning blobless" >&2
    rm -rf "$DIR"/* "$DIR"/.[!.]* 2>/dev/null || true
    rm -rf "$DIR" 2>/dev/null || true
    if [ -n "$SPARSE" ]; then
        $GIT clone -q --filter=blob:none --sparse --no-checkout "$URL" "$DIR"
        ( cd "$DIR" && git sparse-checkout set --no-cone $SPARSE && $GIT checkout -q "$REV" )
    else
        $GIT clone -q --filter=blob:none --no-checkout "$URL" "$DIR"
        ( cd "$DIR" && $GIT checkout -q "$REV" )
    fi
fi

GOT="$(cd "$DIR" && git rev-parse HEAD)"
if [ "$GOT" != "$REV" ]; then
    echo "upstream-fetch: asked for $REV and got $GOT" >&2
    exit 1
fi

# Submodules come after HEAD is verified, since the tree records their revisions, and before
# the patches, which may touch them. Each is checked for content afterwards, because
# `git submodule update` reports success for a module it skipped.
if [ -n "$SUBMODULES" ] && [ -f "$DIR/.gitmodules" ]; then
    echo "upstream: checking out submodules" >&2
    ( cd "$DIR" && $GIT submodule update --init --recursive --depth 1 -q ) || {
        echo "upstream-fetch: submodule checkout failed" >&2
        exit 1
    }
    # A variable rather than a pipe, so the loop runs in this shell and keeps `missing`.
    sub_paths="$(cd "$DIR" && git config -f .gitmodules --get-regexp 'submodule\..*\.path' \
                 2>/dev/null | cut -d' ' -f2-)"
    missing=""
    for path in $sub_paths; do
        # A checked-out module has content; an uninitialised one is an empty directory.
        if [ -z "$(ls -A "$DIR/$path" 2>/dev/null)" ]; then missing="$missing $path"; fi
    done
    if [ -n "$missing" ]; then
        echo "upstream-fetch: submodules are empty after update:$missing" >&2
        exit 1
    fi
fi

# Patches apply to the clean checkout in name order, and one that does not apply stops the build.
if [ -n "$PATCH_DIR" ] && [ -d "$PATCH_DIR" ]; then
    for p in $(ls "$PATCH_DIR"/*.patch 2>/dev/null | sort); do
        echo "upstream: applying $(basename "$p")"
        ( cd "$DIR" && $GIT apply --whitespace=nowarn "$p" ) || {
            echo "upstream-fetch: $(basename "$p") does not apply to $REV" >&2
            exit 1
        }
    done
    check_eol
fi

printf '%s' "$WANT" > "$STAMP"
