#!/bin/sh
#
# Generate the CVAR name macros libultraship expects on the command line, as a header instead.
#
#   gen-lus-cvars.sh [--expect NAME=value] <output.h> <guard> <cmake-file>...
#
# A CVAR name is the key a setting is stored under in the player's configuration file
# (`gSettings.Controllers.Port1.HasConfig`), shared with every other build of that port. A wrong
# prefix compiles and runs but ignores the player's settings. libultraship builds the names from
# macros that CMake supplies through `add_compile_definitions`, which a title here has no CMake to
# supply, so they are read out of upstream's own files and written to a header.
#
# The cmake files are given in the order CMake reads them, and CMake's cache keeps the FIRST
# `set(... CACHE ...)` of a name, so an earlier file wins. A superproject that overrides a prefix
# therefore comes before `libultraship/cmake/cvars.cmake`.
#
# The names emitted are exactly those inside the files' `add_compile_definitions` blocks, so the
# header follows an upstream bump rather than a transcription going stale.
#
# `--expect NAME=value` asserts one resolved value, for a title where the override order is the
# thing that can silently go wrong: Ship of Harkinian's CVAR_PREFIX_CONTROLLERS is `gSettings.
# Controllers` from the superproject and `gControllers` from libultraship's defaults, and both
# compile.
set -eu

EXPECT=""
while [ $# -gt 0 ]; do
    case $1 in
        --expect) EXPECT=${2:?--expect needs NAME=value}; shift 2 ;;
        *) break ;;
    esac
done

OUT=${1:?usage: gen-lus-cvars.sh [--expect NAME=value] <output.h> <guard> <cmake-file>...}
GUARD=${2:?usage: gen-lus-cvars.sh [--expect NAME=value] <output.h> <guard> <cmake-file>...}
shift 2
[ $# -gt 0 ] || { echo "gen-lus-cvars: no cmake files given" >&2; exit 1; }

for f in "$@"; do
    [ -f "$f" ] || { echo "gen-lus-cvars: $f is missing - has upstream moved its CMake files?" >&2
                     exit 1; }
done

# The `add_compile_definitions` blocks are read a second time in the END rule, so awk is handed the
# list as a colon-joined string as well as reading the files for their set() lines.
deflists=$(printf '%s:' "$@" | sed 's/:$//')

tmp="$OUT.tmp"
mkdir -p "$(dirname "$OUT")"

awk -v deflists="$deflists" '
    # set(NAME "value" ...) - the value may contain ${OTHER}, resolved against what is already set.
    # CMake keeps the FIRST value a name is given, so a later set() never overwrites.
    /^[ \t]*set\(CVAR_/ {
        line = $0
        sub(/^[ \t]*set\(/, "", line)
        name = line
        sub(/[ \t].*$/, "", name)
        # The value is the first double-quoted run after the name.
        rest = substr(line, length(name) + 1)
        if (match(rest, /"[^"]*"/) == 0) next
        value = substr(rest, RSTART + 1, RLENGTH - 2)
        if (name in val) next
        # Expand ${OTHER} left to right. One pass is enough: the prefixes are plain strings.
        while (match(value, /\$\{[A-Za-z0-9_]+\}/)) {
            ref = substr(value, RSTART + 2, RLENGTH - 3)
            if (!(ref in val)) {
                printf("gen-lus-cvars: %s refers to ${%s}, which nothing has set\n", name, ref) \
                    > "/dev/stderr"
                bad = 1
                break
            }
            value = substr(value, 1, RSTART - 1) val[ref] substr(value, RSTART + RLENGTH)
        }
        val[name] = value
        next
    }
    END {
        if (bad) exit 1
        # Every file that turns names into definitions contributes, in the same order: a
        # superproject names the prefixes its own sources use, libultraship names its settings.
        # A name in two files is emitted once.
        n = 0
        nfiles = split(deflists, deffile, ":")
        for (fi = 1; fi <= nfiles; fi++) {
            inblock = 0
            while ((getline dline < deffile[fi]) > 0) {
                if (dline ~ /^[ \t]*add_compile_definitions\(/) { inblock = 1; continue }
                if (!inblock) continue
                if (dline ~ /^[ \t]*\)/) { inblock = 0; continue }
                dname = dline
                sub(/^[ \t]*/, "", dname)
                sub(/=.*$/, "", dname)
                if (dname == "") continue
                if (dname !~ /^CVAR_/) continue
                if (dname in emitted) continue
                if (!(dname in val)) {
                    printf("gen-lus-cvars: %s is in add_compile_definitions but nothing set it\n", \
                           dname) > "/dev/stderr"
                    exit 1
                }
                printf("#define %s \"%s\"\n", dname, val[dname])
                emitted[dname] = 1
                n++
            }
            close(deffile[fi])
        }
        if (n == 0) {
            print "gen-lus-cvars: add_compile_definitions block produced no names" > "/dev/stderr"
            exit 1
        }
        printf("gen-lus-cvars: %d CVAR names\n", n) > "/dev/stderr"
    }
' "$@" > "$tmp.body" 2> "$tmp.log" || { cat "$tmp.log" >&2; rm -f "$tmp.body" "$tmp.log"; exit 1; }

{
    cat <<'HDR'
/*
 * Generated from upstream's CMake cvar files by `common/gen-lus-cvars.sh`; do not edit.
 *
 * These are the keys settings are stored under in the player's configuration file, so they are
 * part of an on-disk format shared with every other build of this port - not internal names. A
 * wrong one compiles, runs, and silently ignores that setting.
 */
HDR
    printf '#ifndef %s\n#define %s\n\n' "$GUARD" "$GUARD"
    cat "$tmp.body"
    printf '\n#endif /* %s */\n' "$GUARD"
} > "$tmp"
rm -f "$tmp.body"

if [ -n "$EXPECT" ]; then
    name=${EXPECT%%=*}
    want=${EXPECT#*=}
    if ! grep -q "^#define $name \"$want\"\$" "$tmp"; then
        echo "gen-lus-cvars: $name did not resolve to $want." >&2
        echo "           Got: $(grep "$name" "$tmp" || echo '(absent)')" >&2
        echo "           The cmake files are read in cache order, first set() winning -" >&2
        echo "           check the order they are passed in." >&2
        rm -f "$tmp"
        exit 1
    fi
fi

# Unchanged output leaves the file alone, mtime included. The header is force-included into every
# object, so rewriting it identically would rebuild the whole payload; a caller can therefore run
# this on every make invocation, which is how a header nothing can list as a prerequisite gets made
# before the first compile.
if [ -f "$OUT" ] && cmp -s "$tmp" "$OUT"; then
    rm -f "$tmp" "$tmp.log"
    exit 0
fi

cat "$tmp.log" >&2
rm -f "$tmp.log"
mv "$tmp" "$OUT"
echo "gen-lus-cvars: -> $OUT" >&2
