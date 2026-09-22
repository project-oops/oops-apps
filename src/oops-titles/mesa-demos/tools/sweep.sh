#!/usr/bin/env bash
#
# Build every demo in turn and record one row per demo. See tools/README.md.
#
# The root is derived from this script's own location rather than written down, so the file
# carries no absolute path - which is both what the collection's conventions require and what
# makes it work from any checkout.
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TITLE="$(dirname "$HERE")"

cd "$TITLE" || exit 1

OUT="$TITLE/build/sweep.tsv"
LOG="$TITLE/build/sweep-last.log"

mkdir -p "$TITLE/build"
: > "$OUT"

for f in upstream/src/demos/*.c; do
    d="$(basename "$f" .c)"

    # Each demo links to the same ELF, so the previous one is removed rather than trusted:
    # changing DEMO does not change any source mtime, and make would otherwise call it up to date.
    rm -f build/mesa-demos.elf build/imports.txt

    if ! make DEMO="$d" > "$LOG" 2>&1; then
        err="$(grep -m1 -E 'error:' "$LOG" | sed 's/.*error: //' | cut -c1-90)"
        [ -z "$err" ] && err="$(tail -2 "$LOG" | head -1 | cut -c1-90)"
        printf '%s\tBUILD-FAIL\t%s\n' "$d" "$err" >> "$OUT"
        continue
    fi

    # `make imports` is the useful gate: payload links ignore unresolved symbols, so this is what
    # names a missing function before the console does.
    if ! make DEMO="$d" imports > "$LOG" 2>&1; then
        miss="$(awk '/need a library by hand/{f=1;next} f&&/^    /{gsub(/^ +/,"");printf "%s ",$0} f&&/^$/{exit}' "$LOG" | cut -c1-110)"
        printf '%s\tIMPORTS-FAIL\t%s\n' "$d" "$miss" >> "$OUT"
        continue
    fi

    printf '%s\tOK\t\n' "$d" >> "$OUT"
done

printf 'SWEEP DONE: %s demos -> %s\n' "$(wc -l < "$OUT")" "${OUT#"$TITLE"/}"
