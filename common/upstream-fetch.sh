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

# Patches apply to the clean tree in name order, and one that does not apply stops the build.
# `git apply` works outside a repository too, so an unpacked archive is patched the same way.
apply_patches() {
    if [ -n "$PATCH_DIR" ] && [ -d "$PATCH_DIR" ]; then
        for p in $(ls "$PATCH_DIR"/*.patch 2>/dev/null | sort); do
            echo "upstream: applying $(basename "$p")"
            # The ceiling stops git looking above `$DIR` for a repository. A cloned upstream is
            # its own, so nothing changes there; an unpacked archive is not, and without the
            # ceiling git finds the collection's repository instead, treats the patch's paths as
            # outside the current directory, and skips every hunk while reporting success.
            ( cd "$DIR" && GIT_CEILING_DIRECTORIES="$(cd .. && pwd)" \
                  git -c core.autocrlf=false -c core.eol=lf \
                  apply --whitespace=nowarn "$p" ) || {
                echo "upstream-fetch: $(basename "$p") does not apply to $REV" >&2
                exit 1
            }
        done
    fi
}

case "$KIND" in
git) ;;
# A release download that is not in any repository - SuperTuxKart's data is one. The pin is
# the file's digest, `sha256:<hex>`, which GitHub publishes for a release asset, so pinning
# one needs no download; the fetch refuses a file that does not match it.
archive)
    case "$REV" in
        sha256:[0-9a-f]*) ;;
        *)
            echo "upstream-fetch: an archive's UPSTREAM_REV is its digest, sha256:<hex>, not '$REV'" >&2
            exit 1
            ;;
    esac
    echo "${UPSTREAM_NAME:-upstream}: fetching upstream at ${UPSTREAM_REF:-$REV}"
    echo "upstream: $URL @ $REV"
    DOWNLOAD="$DIR.download"
    rm -f "$DOWNLOAD"
    if command -v curl >/dev/null 2>&1; then
        curl -fL --retry 3 -s -o "$DOWNLOAD" "$URL"
    elif command -v wget >/dev/null 2>&1; then
        wget -q -O "$DOWNLOAD" "$URL"
    elif command -v python3 >/dev/null 2>&1; then
        python3 -c "import urllib.request, sys; urllib.request.urlretrieve(sys.argv[1], sys.argv[2])" "$URL" "$DOWNLOAD"
    elif command -v python >/dev/null 2>&1; then
        python -c "import urllib.request, sys; urllib.request.urlretrieve(sys.argv[1], sys.argv[2])" "$URL" "$DOWNLOAD"
    else
        echo "upstream-fetch: need curl, wget or python3 to download $URL" >&2
        exit 1
    fi || {
        echo "upstream-fetch: download failed: $URL" >&2
        rm -f "$DOWNLOAD"
        exit 1
    }
    if command -v sha256sum >/dev/null 2>&1; then
        GOT="sha256:$(sha256sum "$DOWNLOAD" | cut -d' ' -f1)"
    elif command -v shasum >/dev/null 2>&1; then
        GOT="sha256:$(shasum -a 256 "$DOWNLOAD" | cut -d' ' -f1)"
    elif command -v python3 >/dev/null 2>&1; then
        GOT="sha256:$(python3 -c "import hashlib, sys; print(hashlib.sha256(open(sys.argv[1], 'rb').read()).hexdigest())" "$DOWNLOAD")"
    else
        GOT="sha256:$(sha256sum "$DOWNLOAD" | cut -d' ' -f1)"
    fi
    if [ "$GOT" != "$REV" ]; then
        echo "upstream-fetch: asked for $REV and got $GOT" >&2
        rm -f "$DOWNLOAD"
        exit 1
    fi
    rm -rf "$DIR"/* "$DIR"/.[!.]* 2>/dev/null || true
    rm -rf "$DIR" 2>/dev/null || true
    mkdir -p "$DIR"
    # A tarball can hold symbolic links, some dangling until a build generates their target
    # (NetSurf's bundle links `res/Messages` to an `en/Messages` its build writes). Git Bash's
    # tar makes a link by copying the target, which fails for those; `winsymlinks:sys` makes it
    # write the link itself instead. The setting means nothing on a platform with real links.
    case "$URL" in
        *.zip)
            if command -v unzip >/dev/null 2>&1; then
                unzip -q "$DOWNLOAD" -d "$DIR"
            elif command -v python3 >/dev/null 2>&1; then
                python3 -m zipfile -e "$DOWNLOAD" "$DIR"
            elif command -v python >/dev/null 2>&1; then
                python -m zipfile -e "$DOWNLOAD" "$DIR"
            else
                echo "upstream-fetch: need unzip or python3 to unpack $DOWNLOAD" >&2
                exit 1
            fi
            ;;
        *) MSYS="${MSYS:+$MSYS }winsymlinks:sys" tar -xf "$DOWNLOAD" -C "$DIR" ;;
    esac || {
        echo "upstream-fetch: $DOWNLOAD did not unpack" >&2
        exit 1
    }
    # Those `sys` links are only links to Git Bash: the build container reads each as a small
    # file holding a cookie. So on Windows every link that resolves becomes a copy of what it
    # points at (NetSurf's `res/throbber` is a directory in another front end), and a dangling
    # one is left for the build that generates its target.
    case "$(uname -o 2>/dev/null)" in
        Msys | Cygwin)
            find "$DIR" -type l | while read -r link; do
                [ -e "$link" ] || continue
                cp -rL "$link" "$link.oops-copy" && rm -f "$link" && mv "$link.oops-copy" "$link"
            done
            ;;
    esac
    rm -f "$DOWNLOAD"
    apply_patches
    printf '%s' "$WANT" > "$STAMP"
    exit 0
    ;;
*)
    echo "upstream-fetch: unknown kind '$KIND' - the lock's UPSTREAM_KIND must be one this" >&2
    echo "                script implements: git or archive. Add another here rather than in" >&2
    echo "                a title." >&2
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
    # The tree's gitlinks, not .gitmodules: that file is ordinary tracked content and can name a
    # module the revision does not contain, which nothing can check out and which would then fail this
    # check forever. The gitlinks are what `git submodule update` acts on.
    #
    # A variable rather than a pipe, so the loop runs in this shell and keeps `missing`. `ls-tree`
    # separates the path with a tab, hence `cut -f2-`.
    sub_paths="$(cd "$DIR" && $GIT ls-tree -r HEAD 2>/dev/null \
                 | grep -E '^[0-7]+ commit ' | cut -f2-)"
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

apply_patches
if [ -n "$PATCH_DIR" ] && [ -d "$PATCH_DIR" ]; then
    check_eol
fi

printf '%s' "$WANT" > "$STAMP"
