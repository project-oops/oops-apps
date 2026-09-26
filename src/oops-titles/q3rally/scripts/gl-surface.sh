#!/usr/bin/env bash
# gl-surface.sh - which GL entry points this title resolves *by name*, and whether oops-gl answers.
#
#   gl-surface.sh <qgl.h> <oops-sdk/src/gl>
#
# # Why a link with no undefined symbols does not answer this
#
# **ioquake3 does not call GL by symbol.** `sdl_glimp.c:269` fills every `qgl*` pointer from a
# *string* through `SDL_GL_GetProcAddress`, including core 1.1 names like `glBindTexture` that a
# normal program would just call:
#
#     #define GLE( ret, name, ... ) qgl##name = (name##proc *) SDL_GL_GetProcAddress("gl" #name); \
#         if ( qgl##name == NULL ) { ...ERROR: Missing OpenGL function...; success = qfalse; }
#
# One NULL fails `GLimp_GetProcAddresses`, and the renderer refuses to start. So the payload can
# link with nothing undefined - because every one of those functions *is* linked in - and still not
# draw a frame, because the lookup that finds them is a table in `gl_procs.h` and a name absent from
# that table answers NULL. That is the gap this script measures, and it is a gap that only a title
# resolving core GL by name has.
#
# The required sets are the ones `GLimp_GetProcAddresses` takes for a fixed-function context
# reporting GL 1.1, which is what `oops-gl` reports by default (`gl_state.c:1844`). `QGL_3_0_PROCS`
# - `glGetStringi` among them - is bound only when the context claims 3.0, so it is listed here as
# "not asked for" rather than missing.
#
# # The table is read by expanding it, not by grepping the file that holds it
#
# **This grepped `gl_procs.h` at first and stopped being able to pass.** The core names moved into
# `gl_procs_core.h` the same hour, and the check went on reading the old file and reporting all 66
# missing - an arm that answers the same way whatever the truth is. So the list is taken from the
# preprocessor: the same macro the real table is built from, expanded by the same compiler,
# whatever files it happens to be spread across.
set -euo pipefail

QGL="${1:?qgl.h}"
GLDIR="${2:?oops-sdk/src/gl}"
CC="${CC:-clang}"

EXPANDED="$(mktemp)"
trap 'rm -f "$EXPANDED" "$EXPANDED.c"' EXIT
# **Bracketed, because the expansion is one long line.** `X(n) #n` alone gives
# `"glAccum" "glAlphaFunc" ...` with nothing to say where one entry ends - and `XS` emits two
# adjacent literals for one name, which is then indistinguishable from two names. The brackets put
# the boundary in.
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
