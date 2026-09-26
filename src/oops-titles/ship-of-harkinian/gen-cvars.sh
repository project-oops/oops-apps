#!/bin/sh
#
# Generate `ship/cvars_generated.h` - the CVAR name macros libultraship expects on the command
# line, as a header instead.
#
#   gen-cvars.sh <upstream-dir> <output.h>
#
# A CVAR name is the key a setting is stored under in the player's configuration file
# (`gSettings.Controllers.Port1.HasConfig`), shared with other Ship of Harkinian builds. A
# wrong prefix compiles and runs but ignores the player's settings. libultraship builds the
# names from macros that CMake supplies through `add_compile_definitions`.
#
# The values come from three CMake files, read in this order; CMake's cache keeps the first
# `set(... CACHE ...)` of a name:
#
#   CMake/soh-cvars.cmake            the prefixes - CVAR_PREFIX_SETTING is "gSettings"
#   CMake/lus-cvars.cmake            the superproject's values, built from those prefixes
#   libultraship/cmake/cvars.cmake   libultraship's standalone defaults, and the list of
#                                    names that become definitions
#
# The third file's values lose: its CVAR_PREFIX_CONTROLLERS is `gControllers`, the shipped
# value `gSettings.Controllers`. The script emits exactly the names in that file's
# `add_compile_definitions`, so it follows upstream bumps.
set -eu

U=${1:?usage: gen-cvars.sh <upstream-dir> <output.h>}
OUT=${2:?usage: gen-cvars.sh <upstream-dir> <output.h>}

SOH_CVARS="$U/CMake/soh-cvars.cmake"
LUS_CVARS="$U/CMake/lus-cvars.cmake"
LUS_DEFAULTS="$U/libultraship/cmake/cvars.cmake"
for f in "$SOH_CVARS" "$LUS_CVARS" "$LUS_DEFAULTS"; do
    [ -f "$f" ] || { echo "gen-cvars: $f is missing - has upstream moved its CMake files?" >&2; exit 1; }
done

tmp="$OUT.tmp"
mkdir -p "$(dirname "$OUT")"

awk -v deflists="$LUS_DEFAULTS:$SOH_CVARS" '
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
                printf("gen-cvars: %s refers to ${%s}, which nothing has set\n", name, ref) > "/dev/stderr"
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
        # The names to emit are the ones upstream turns into definitions, from both files that do
        # so: the libultraship list, and the soh prefixes, which the soh sources use directly
        # through cvar_prefixes.h. A name in both is emitted once.
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
                if (dname in emitted) continue
                if (!(dname in val)) {
                    printf("gen-cvars: %s is in add_compile_definitions but nothing set it\n", dname) > "/dev/stderr"
                    exit 1
                }
                printf("#define %s \"%s\"\n", dname, val[dname])
                emitted[dname] = 1
                n++
            }
            close(deffile[fi])
        }
        if (n == 0) {
            print "gen-cvars: add_compile_definitions block produced no names" > "/dev/stderr"
            exit 1
        }
        printf("gen-cvars: %d CVAR names\n", n) > "/dev/stderr"
    }
' "$SOH_CVARS" "$LUS_CVARS" "$LUS_DEFAULTS" > "$tmp.body"

{
    cat <<'HDR'
/*
 * Generated from upstream's CMake cvar files by `gen-cvars.sh`; do not edit.
 *
 * These are the keys settings are stored under in the player's configuration file, so they are
 * part of an on-disk format shared with every other Ship of Harkinian build - not internal names.
 * A wrong one compiles, runs, and silently ignores that setting.
 */
#ifndef OOPS_SOH_CVARS_GENERATED_H
#define OOPS_SOH_CVARS_GENERATED_H

HDR
    cat "$tmp.body"
    cat <<'TAIL'

#endif /* OOPS_SOH_CVARS_GENERATED_H */
TAIL
} > "$tmp"
rm -f "$tmp.body"

# CVAR_PREFIX_CONTROLLERS is the one name whose value differs between the superproject and
# libultraship's defaults, so it proves the first-set-wins order was applied.
if ! grep -q '^#define CVAR_PREFIX_CONTROLLERS "gSettings\.Controllers"$' "$tmp"; then
    echo "gen-cvars: CVAR_PREFIX_CONTROLLERS did not resolve to gSettings.Controllers." >&2
    echo "           Got: $(grep 'CVAR_PREFIX_CONTROLLERS' "$tmp" || echo '(absent)')" >&2
    echo "           The superproject's CMake/lus-cvars.cmake must win over" >&2
    echo "           libultraship/cmake/cvars.cmake - check the cache-order rule in this script." >&2
    rm -f "$tmp"
    exit 1
fi

mv "$tmp" "$OUT"
echo "gen-cvars: -> $OUT"
