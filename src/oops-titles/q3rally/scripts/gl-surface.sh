#!/usr/bin/env bash
# gl-surface.sh - which GL entry points this title resolves by name, and whether oops-gl answers.
#
#   gl-surface.sh <qgl.h> <oops-sdk/src/gl>
#
# `sdl_glimp.c:269` fills every `qgl*` pointer by name through `SDL_GL_GetProcAddress`, core
# 1.1 names included, and one NULL fails `GLimp_GetProcAddresses`. A clean link cannot show
# this; the answer is whether each name is in oops-gl's by-name table.
#
# The required sets are those bound for a fixed-function GL 1.1 context, which oops-gl
# reports by default. `QGL_3_0_PROCS` is bound only for a 3.0 context and is listed apart.
# The table is read by expanding `OOPS_GL_PROC_LIST` with the preprocessor, so it holds
# whichever files the table spans.
set -euo pipefail

QGL="${1:?qgl.h}"
GLDIR="${2:?oops-sdk/src/gl}"
CC="${CC:-clang}"

EXPANDED="$(mktemp)"
trap 'rm -f "$EXPANDED" "$EXPANDED.c"' EXIT
# Each entry is bracketed: the expansion is one line, and `XS` emits two adjacent literals
# for one name.
cat > "$EXPANDED.c" <<'C'
#include "gl_procs.h"
#define X(n) [#n]
#define XS(n, s) [#n #s]
OOPS_GL_PROC_LIST(X, XS)
C
"$CC" -E -P -I "$GLDIR" "$EXPANDED.c" > "$EXPANDED"

python3 - "$QGL" "$EXPANDED" <<'PY'
import re, sys

qgl_path, expanded_path = sys.argv[1], sys.argv[2]

# The sets a fixed-function context at GL 1.1 binds, from sdl_glimp.c:300-305.
REQUIRED = ["QGL_1_1_PROCS", "QGL_1_1_FIXED_FUNCTION_PROCS",
            "QGL_DESKTOP_1_1_PROCS", "QGL_DESKTOP_1_1_FIXED_FUNCTION_PROCS"]
# Bound only when the context claims 3.0 or ES 3.0, which oops-gl does not.
OPTIONAL = ["QGL_3_0_PROCS"]

text = open(qgl_path, encoding="utf-8", errors="replace").read().replace("\r\n", "\n")

def macro_body(name):
    """The continued lines of `#define <name> \\ ... `, joined."""
    m = re.search(r"^#define\s+" + re.escape(name) + r"\s*\\\n", text, re.M)
    if not m:
        sys.exit("gl-surface: no #define %s in %s" % (name, qgl_path))
    out, i = [], m.end()
    for line in text[i:].split("\n"):
        out.append(line)
        if not line.rstrip().endswith("\\"):
            break
    return "\n".join(out)

def names(macro):
    # GLE(ret, Name, args...) -> glName
    return ["gl" + n for n in re.findall(r"GLE\(\s*[^,]+,\s*([A-Za-z0-9_]+)", macro_body(macro))]

required, optional = [], []
for m in REQUIRED:
    required += names(m)
for m in OPTIONAL:
    optional += names(m)

# The lookup table, as the preprocessor expands it: a run of "glFoo" "glFooARB" string literals.
# Adjacent literals are how XS spells a suffixed name, so they are concatenated the way C would.
exp = open(expanded_path, encoding="utf-8", errors="replace").read()
have = set()
for group in re.findall(r"\[([^\]]*)\]", exp):
    have.add("".join(re.findall(r'"([^"]*)"', group)))
if not have:
    sys.exit("gl-surface: the expansion produced no names - the include path is probably wrong")

missing = [n for n in required if n not in have]
print("q3rally resolves %d GL entry points by name; oops-gl's table answers %d"
      % (len(required), len(required) - len(missing)))
if optional:
    print("  (%d more in QGL_3_0_PROCS are bound only by a context claiming 3.0, which this is not: %s)"
          % (len(optional), ", ".join(optional)))
if missing:
    print("\n-- %d absent from OOPS_GL_PROC_LIST, each one a 'Missing OpenGL function' at startup --"
          % len(missing))
    for i in range(0, len(missing), 4):
        print("   " + "  ".join("%-28s" % n for n in missing[i:i + 4]).rstrip())
    sys.exit(1)
print("every name this title asks for is in the table")
PY
