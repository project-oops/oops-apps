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

# **An origin whose own build needs its submodules.** `UPSTREAM_SUBMODULES=1` in the lock checks
# them out after the revision lands; a lock without it behaves exactly as before.
#
# Opt-in rather than automatic, because most origins here either have no submodules or have ones
# their build does not use, and recursing costs a clone per module. Ship of Harkinian is the first
# that cannot build without them: `libultraship` is its entire renderer and `torch` is the asset
# processor, and a checkout without them is a tree that configures and then fails on missing
# headers - the kind of "fetched, but not really" this script's stamp comment is about.
#
# **The pin still holds.** A submodule's revision is recorded in the superproject's tree, so
# checking out the pinned commit and then updating submodules lands exactly the revisions that
# commit names. There is no second hash to keep in the lock and none to drift.
SUBMODULES="${UPSTREAM_SUBMODULES:-}"

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
# In the stamp for the same reason `$SPARSE` is: turning submodules on for a tree already fetched
# without them has to fetch again, or the lock change is another no-op that reports success.
WANT="$REV $(patch_sum) $(sparse_key) sub=${SUBMODULES:-0}"

# **The tree is verified after `git apply`, which is the only step that can reintroduce CRLF.**
#
# `git ls-files --eol` compares the index against the working tree; `i/lf w/crlf` means git rewrote
# the file and the tree is no longer the revision the lock names.
#
# Why it is fatal rather than a warning: a CRLF data file does not fail to build, it fails at run
# time, somewhere unrelated, on hardware. See the note above `$GIT`.
#
# **It is not run after the checkout, because the checkout cannot produce CRLF.** `$GIT` is
# `git -c core.autocrlf=false -c core.eol=lf`, and `-c` overrides both the machine's global config
# and the repository's own - which is the whole point of the wrapper, and covers the two causes
# named in its note. Checking the result of a command that was just forced to produce it is not
# evidence of anything, and on gl-cts it is not cheap either: **over an hour**, because `--eol`
# classifies a file by reading it and that tree is 8202 files and 209 MB across the WSL drvfs
# boundary. It blocked a build at 01:02 on 2026-09-25 having proved nothing; gl-cts has no
# `patches/` at all, so there was no step after the checkout that could have changed a byte.
#
# `git apply` is different and is still checked below: it honours the repository's own eol
# settings and is not routed through `$GIT`'s `-c` overrides in the way a checkout is, so a patch
# genuinely can put CRLF into a tree that landed clean. That is the call that earned its keep -
# Extreme Tux Racer lost its fonts, music and six textures to CRLF on 2026-09-24.
#
# **It used to run on the stamp-matched fast path too, and that is not affordable.** `--eol`
# classifies a file by *reading it*, so the cost is the tree's bytes rather than its file count,
# and it was paid by every `make` including a no-op one. Measured on gl-cts, the largest origin
# here - 8202 files, 209 MB - under WSL against `/mnt/c`: **66 MB read in 19 minutes, 3 seconds of
# CPU**, all of it blocked on the drvfs boundary, for an unchanged tree that had just been
# verified. An incremental build of that title was 17 seconds before it and had not finished
# parsing after 18 minutes with it.
#
# Nothing is lost by dropping it there. The tree's line endings can only change when git writes
# the tree, which is the checkout below and `git apply` after it, and both are still checked. A
# tree that predates the `$GIT` wrapper is the one case the fast path did cover; that is a
# one-time migration rather than a per-build risk, so it is `UPSTREAM_CHECK_EOL=1` on demand
# instead of a tax on everyone forever.
check_eol() {
    crlf="$(cd "$DIR" && git ls-files --eol 2>/dev/null | grep -c 'i/lf[[:space:]]*w/crlf' || true)"
    [ "${crlf:-0}" -gt 0 ] || return 0
    echo "upstream-fetch: $crlf files in $DIR have CRLF in the working tree and LF in the index." >&2
    echo "                This tree is not the revision the lock names, and a program that reads its" >&2
    echo "                own data files will fail on the extra byte - Extreme Tux Racer lost its" >&2
    echo "                fonts, music and six textures to exactly this on 2026-09-24." >&2
    echo "                Run 'make upstream-clean' and build again; the fetch now checks out with" >&2
    echo "                core.autocrlf=false." >&2
    exit 1
}

if [ -f "$STAMP" ] && [ "$(cat "$STAMP")" = "$WANT" ]; then
    # Opt-in only. See the note above `check_eol` for what this used to cost every build.
    # An `if` rather than `[ ... ] && check_eol`, because `set -e` is on and the exit status of a
    # false AND-OR list is the kind of thing that is argued about rather than known.
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

# **No `git`, no fetch - refused here, before anything is deleted.**
#
# Everything below this line assumes `git` exists: the checkout, both fallbacks, and the two `rm`s
# that clear the way for them. Without it the first attempt fails, the fallback empties the
# directory and then fails too, and what was a perfectly good checkout a moment ago is gone - with
# the only clue being four "command not found" lines scrolling past.
#
# That is not hypothetical. The `silkeh/clang:21` container carries no `git`, so any `make` that
# reaches a fetch inside it destroys the tree the host had fetched. It cost Ship of Harkinian's
# 145 MB checkout on 2026-09-25, twice, before the cause was read rather than guessed at.
#
# Refusing leaves the existing tree untouched, which is the outcome a caller that cannot fetch
# should get: a stale tree it can still build from beats no tree at all.
if ! command -v git >/dev/null 2>&1; then
    echo "upstream-fetch: no git on PATH - refusing, so the existing checkout is left alone" >&2
    echo "upstream-fetch: run the fetch on the host; the clang container carries no git" >&2
    exit 1
fi
# **Emptied rather than removed, because on Windows the directory itself is often not removable.**
#
# A fetched tree has just been read by a build, an editor or a container mount, and Windows keeps
# the *directory* handle alive after its contents are gone. `rm -rf "$DIR"` then fails with "Device
# or resource busy" while leaving nothing behind, and under `set -e` that aborts a fetch that had
# no reason to fail - the tree was already empty and ready to be filled. Removing the contents and
# reusing the directory is the same outcome by a route the platform allows. The `.[!.]*` glob is
# what takes `.git` and the stamp with it; without it the next `git init` would inherit a config
# and a remote from the tree being replaced.
rm -rf "$DIR"/* "$DIR"/.[!.]* 2>/dev/null || true
rm -rf "$DIR" 2>/dev/null || true
mkdir -p "$DIR"

# `SPARSE` is set above the stamp, which is the only thing that reads it before this point.

# **Every git call below is wrapped in `$GIT`, and the reason is line endings.**
#
# A fetched tree is not source we maintain - it is a copy of somebody else's revision, and it has to
# land byte-for-byte as they published it. On a machine with `core.autocrlf=true` (the Windows git
# default, and set globally on at least one machine here) git rewrites every file it decides is text
# to CRLF **on checkout**, so what lands is not the revision the lock names: `git ls-files --eol`
# reports `i/lf w/crlf` and the working tree differs from the blob by one byte per line.
#
# That is not cosmetic and it is not confined to compiling. On 2026-09-24 it cost Extreme Tux Racer
# its fonts, its music and six of its textures on hardware: the game's `.lst` files take the value
# after `[file]` to the end of the line, so a CR became part of every filename whose field ended a
# line, and `open()` failed on all of them. Nothing above the syscall could see why - upstream's own
# message is `error FT_New_Face`.
#
# `core.eol=lf` is set as well as `autocrlf=false`, because a repository carrying its own
# `.gitattributes` with `text=auto` would otherwise still convert.
GIT="git -c core.autocrlf=false -c core.eol=lf"

# **Shallow first, blobless second.** Asking for one commit is the cheapest thing that can work
# and most servers allow it; a server that does not (`uploadpack.allowReachableSHA1InWant` off)
# fails here rather than silently fetching something else, and the fallback clones history
# without file contents and fills them in on checkout. Either way what lands is the revision the
# lock names.
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
    # Same reasoning as the removal above: empty it, and only then try to remove it.
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

# No `check_eol` here. The checkout above went through `$GIT`, which forces LF; see the note on
# `check_eol` for why verifying that costs an hour on gl-cts and proves nothing.

GOT="$(cd "$DIR" && git rev-parse HEAD)"
if [ "$GOT" != "$REV" ]; then
    echo "upstream-fetch: asked for $REV and got $GOT" >&2
    exit 1
fi

# **The submodules, after the revision is verified and before the patches.**
#
# After, because their revisions come from the superproject's tree and that is only settled once
# HEAD is where the lock asked. Before the patches, because a patch may well touch a file inside
# one - which is the whole reason a title would carry patches against a project whose renderer is
# a submodule.
#
# `--depth 1` for the same reason the superproject is shallow: history is not what is being built.
# Checked afterwards rather than trusted, because `git submodule update` reports success for a
# module it decided to skip, and a renderer directory that is present-but-empty is exactly the
# failure this script exists to make loud.
if [ -n "$SUBMODULES" ] && [ -f "$DIR/.gitmodules" ]; then
    echo "upstream: checking out submodules" >&2
    ( cd "$DIR" && $GIT submodule update --init --recursive --depth 1 -q ) || {
        echo "upstream-fetch: submodule checkout failed" >&2
        exit 1
    }
    # **The declared paths, read into a variable rather than piped or fed by a heredoc.** A pipe
    # would run the loop in a subshell and lose `missing`; a heredoc carrying the command
    # substitution is what broke this script's syntax the first time it was written. A plain `for`
    # over an unquoted variable is enough, because submodule paths have no spaces in them - and if
    # one ever did, the emptiness check below would simply report it as missing rather than pass.
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

# **Patches are applied to a clean checkout, in name order, and a failure stops the build.** A
# patch that no longer applies means upstream moved under it, and the answer is to rebase the
# patch or move what it did into `shim/` - never to carry on with half of it applied.
if [ -n "$PATCH_DIR" ] && [ -d "$PATCH_DIR" ]; then
    for p in $(ls "$PATCH_DIR"/*.patch 2>/dev/null | sort); do
        echo "upstream: applying $(basename "$p")"
        ( cd "$DIR" && $GIT apply --whitespace=nowarn "$p" ) || {
            echo "upstream-fetch: $(basename "$p") does not apply to $REV" >&2
            exit 1
        }
    done
    # Patches land through `git apply`, which honours the repository's own eol settings, so the
    # tree is checked again after them rather than only after the checkout.
    check_eol
fi

printf '%s' "$WANT" > "$STAMP"
