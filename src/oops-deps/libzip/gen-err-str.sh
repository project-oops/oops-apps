#!/bin/sh
#
# Generate libzip's `zip_err_str.c`, which upstream generates too.
#
#   gen-err-str.sh <upstream-dir> <output.c>
#
# The table pairs each `ZIP_ER_*` number in upstream's headers with its message, so it is
# generated from the pinned headers rather than checked in. This is the transform of
# `cmake/GenerateZipErrorStrings.cmake` in awk. A line such as
#
#   #define ZIP_ER_SEEK 4             /* S Seek error */
#
# gives `{ S, "Seek error" },` - the letter is the error's type (`L`/`N`/`S`/`Z`, or `E`/`G`
# for the detail table) and the rest of the comment is the message.
set -eu

U=${1:?usage: gen-err-str.sh <upstream-dir> <output.c>}
OUT=${2:?usage: gen-err-str.sh <upstream-dir> <output.c>}

emit_table() {
    # $1 = header, $2 = define prefix, $3 = the letters that are a type in this table
    awk -v prefix="$2" -v letters="$3" '
        index($0, "#define " prefix) == 1 {
            # Everything between the comment markers, then the leading type letter.
            i = index($0, "/*"); if (i == 0) next
            body = substr($0, i + 2)
            j = index(body, "*/"); if (j == 0) next
            body = substr(body, 1, j - 1)
            sub(/^[ \t]+/, "", body)
            type = substr(body, 1, 1)
            if (index(letters, type) == 0) next
            if (substr(body, 2, 1) != " ") next
            msg = substr(body, 3)
            sub(/[ \t]+$/, "", msg)
            printf("    { %s, \"%s\" },\n", type, msg)
        }
    ' "$1"
}

tmp="$OUT.tmp"
{
    cat <<'HDR'
/*
  This file was generated automatically from zip.h and zipint.h; make changes there.
*/

#include "zipint.h"

#define L ZIP_ET_LIBZIP
#define N ZIP_ET_NONE
#define S ZIP_ET_SYS
#define Z ZIP_ET_ZLIB

#define E ZIP_DETAIL_ET_ENTRY
#define G ZIP_DETAIL_ET_GLOBAL

const struct _zip_err_info _zip_err_str[] = {
HDR
    emit_table "$U/lib/zip.h" "ZIP_ER_" "LNSZ"
    cat <<'MID'
};

const int _zip_err_str_count = sizeof(_zip_err_str)/sizeof(_zip_err_str[0]);

const struct _zip_err_info _zip_err_details[] = {
MID
    emit_table "$U/lib/zipint.h" "ZIP_ER_DETAIL_" "EG"
    cat <<'TAIL'
};

const int _zip_err_details_count = sizeof(_zip_err_details)/sizeof(_zip_err_details[0]);
TAIL
} > "$tmp"

# `zip_strerror` indexes the table by error number, so the defines must be numbered exactly
# 0..n-1 in order and the table must have n rows. A gap would shift every later message.
check_order() {
    # $1 = header, $2 = prefix, $3 = how many rows the table got, $4 = table name
    awk -v prefix="$2" -v got="$3" -v name="$4" '
        index($0, "#define " prefix) == 1 {
            if ($3 != n) {
                printf("gen-err-str: %s is not densely numbered - expected %d at %s, found %s\n",
                       name, n, $2, $3) > "/dev/stderr"
                bad = 1
            }
            n++
        }
        END {
            if (n != got) {
                printf("gen-err-str: %s has %d defines but the table got %d rows.\n",
                       name, n, got) > "/dev/stderr"
                printf("             The upstream comment format changed; re-check against\n") > "/dev/stderr"
                printf("             cmake/GenerateZipErrorStrings.cmake.\n") > "/dev/stderr"
                bad = 1
            }
            exit bad ? 1 : 0
        }
    ' "$1"
}

got_err=$(awk '/^const struct _zip_err_info _zip_err_str/,/^};/' "$tmp" | grep -c '^    {' || true)
got_det=$(awk '/^const struct _zip_err_info _zip_err_details/,/^};/' "$tmp" | grep -c '^    {' || true)
if ! check_order "$U/lib/zip.h" "ZIP_ER_" "$got_err" "zip.h ZIP_ER_" ||
   ! check_order "$U/lib/zipint.h" "ZIP_ER_DETAIL_" "$got_det" "zipint.h ZIP_ER_DETAIL_"; then
    rm -f "$tmp"
    exit 1
fi

mv "$tmp" "$OUT"
echo "libzip: $got_err error strings, $got_det details -> $OUT"
