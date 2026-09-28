#!/usr/bin/env python3
"""The keyword lookups libhubbub and libsvgtiny ask gperf for, without gperf.

    gperf_lite.py <input.gperf> <output.c>

Each library includes a C file its build generates with gperf from a `.gperf` keyword list
(`libhubbub/src/treebuilder/element-type.gperf`, `libsvgtiny/src/colors.gperf`). gperf is not
in the build image, so this writes a file with the same contract: the declarations block
copied through, a table of the entries in the struct the input names, and the lookup function
under the name the input defines, returning the entry whose keyword is exactly the `len` bytes
at `str`, or NULL. gperf finds it through a perfect hash; this through a binary search over the
sorted table - the same answers, in logarithmic rather than constant time, for 107 element
names and 147 colours.

Only what the two inputs use is implemented: `%struct-type`, `%ignore-case` (ASCII),
`%global-table` (the table is then `wordlist`, as gperf names it), `%define
lookup-function-name`, and a `struct <name>;` line naming the struct. Any other
directive that would change a lookup's answer stops the script, rather than being ignored.
"""
import re
import sys

# Directives that change only how gperf builds its table, not what a lookup answers.
HARMLESS = {"language", "compare-strncmp", "readonly-tables", "global-table", "switch",
            "struct-type", "ignore-case", "define"}


def main(src, dst):
    text = open(src, encoding="utf-8").read()
    head, sep, rest = text.partition("\n%%\n")
    if not sep:
        sys.exit(f"{src}: no %% section separator")
    keywords = rest.split("\n%%")[0]

    ignore_case = False
    global_table = False
    lookup = None
    struct = None
    preamble = []
    in_code = False
    for line in head.splitlines():
        if line.startswith("%{"):
            in_code = True
            continue
        if line.startswith("%}"):
            in_code = False
            continue
        if in_code:
            preamble.append(line)
            continue
        m = re.match(r"%(\S+)(?:\s+(.*))?$", line)
        if m:
            name, arg = m.group(1).split("=")[0], m.group(2) or ""
            if name not in HARMLESS:
                sys.exit(f"{src}: gperf directive %{name} is not implemented here")
            if name == "ignore-case":
                ignore_case = True
            if name == "global-table":
                global_table = True
            if name == "define" and arg.startswith("lookup-function-name"):
                lookup = arg.split()[1]
            continue
        m = re.match(r"struct\s+(\w+)\s*;", line.strip())
        if m:
            struct = m.group(1)

    if not lookup or not struct:
        sys.exit(f"{src}: needs a lookup-function-name and a struct line")

    entries = []
    for line in keywords.splitlines():
        if not line.strip() or line.startswith("#"):
            continue
        key, _, value = line.partition(",")
        entries.append((key.strip(), value.strip()))
    fold = (lambda s: s.lower()) if ignore_case else (lambda s: s)
    entries.sort(key=lambda e: fold(e[0]).encode())
    folded = [fold(k) for k, _ in entries]
    if len(set(folded)) != len(folded):
        sys.exit(f"{src}: duplicate keywords")

    out = [f"/* Generated from {src.split('/')[-1]} by shim/gperf_lite.py. */"]
    out += preamble
    if ignore_case:
        out.append("#include <strings.h> /* strncasecmp */")
    # `%global-table` puts gperf's table at file scope as `wordlist`, and a file that includes the
    # output may walk it (hubbub's reverse lookup, type to name, does).
    table = "wordlist" if global_table else f"{lookup}_table"
    out.append(f"static const struct {struct} {table}[] = {{")
    out += [f'\t{{"{k}", {v}}},' for k, v in entries]
    out.append("};")
    cmp = "strncasecmp" if ignore_case else "strncmp"
    out.append(f"""
const struct {struct} *{lookup}(const char *str, size_t len);
const struct {struct} *{lookup}(const char *str, size_t len)
{{
\tsize_t lo = 0, hi = sizeof({table}) / sizeof({table}[0]);
\twhile (lo < hi) {{
\t\tconst size_t mid = lo + (hi - lo) / 2;
\t\tconst char *key = {table}[mid].name;
\t\tconst size_t klen = strlen(key);
\t\tint c = {cmp}(str, key, len < klen ? len : klen);
\t\tif (c == 0)
\t\t\tc = (len > klen) - (len < klen);
\t\tif (c == 0)
\t\t\treturn &{table}[mid];
\t\tif (c < 0)
\t\t\thi = mid;
\t\telse
\t\t\tlo = mid + 1;
\t}}
\treturn NULL;
}}
""")
    new = "\n".join(out)
    try:
        if open(dst, encoding="utf-8").read() == new:
            return
    except OSError:
        pass
    open(dst, "w", encoding="utf-8").write(new)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
