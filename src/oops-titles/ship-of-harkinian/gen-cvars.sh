#!/bin/sh
#
# Generate `ship/cvars_generated.h` - the CVAR name macros libultraship expects on the command
# line, as a header instead.
#
#   gen-cvars.sh <upstream-dir> <output.h>
#
# # What these are
#
# A CVAR name is the key a setting is stored under in the user's configuration file:
# `gSettings.Controllers.Port1.HasConfig`. libultraship writes them as macros concatenated with
# string literals (`CVAR_PREFIX_CONTROLLERS ".Port%d.HasConfig"`), and CMake supplies the macros
# through `add_compile_definitions`. 31 of the runtime's 138 sources stop without them.
#
# **These are not internal names, and getting one wrong is not a compile error.** They are the
# on-disk format of a save file that other Ship of Harkinian builds also read. A build that
# invented its own prefix would run perfectly and silently ignore every setting the player had.
#
# # Why a generator and not a list
#
# The values come from three CMake files, in this order, under CMake's cache rule that **the first
# `set(... CACHE ...)` wins**:
#
#   CMake/soh-cvars.cmake            the prefixes - CVAR_PREFIX_SETTING is "gSettings"
#   CMake/lus-cvars.cmake            the superproject's values, built from those prefixes
#   libultraship/cmake/cvars.cmake   libultraship's standalone defaults, and the list of which
#                                    names actually become definitions
#
# The third file is where a reader would naturally look, and it is the one whose values are
# **not** used: `CVAR_PREFIX_CONTROLLERS` is `gControllers` there and `gSettings.Controllers` in
# the build SoH ships. A transcribed list would have taken the wrong one.
#
# So this resolves the chain rather than recording its result, and emits exactly the names in
# `add_compile_definitions`, so a bump that adds one is picked up and a bump that removes one does
# not leave a stale macro behind.
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

awk -v defaults="$LUS_DEFAULTS" '
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
        # The names to emit are exactly the ones libultraship turns into definitions.
        n = 0
        while ((getline dline < defaults) > 0) {
            if (dline ~ /^[ \t]*add_compile_definitions\(/) { inblock = 1; continue }
            if (!inblock) continue
            if (dline ~ /^[ \t]*\)/) { inblock = 0; continue }
            dname = dline
            sub(/^[ \t]*/, "", dname)
            sub(/=.*$/, "", dname)
            if (dname == "") continue
            if (!(dname in val)) {
                printf("gen-cvars: %s is in add_compile_definitions but nothing set it\n", dname) > "/dev/stderr"
                exit 1
            }
            printf("#define %s \"%s\"\n", dname, val[dname])
            n++
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

# **Spot-check one resolved value, because the failure here is silent.** If the cache-order rule
# were implemented backwards this would say `gControllers`, the file would compile, and the port
# would read nobody else's settings. Checking the one name that differs between the two files
# catches exactly that.
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
