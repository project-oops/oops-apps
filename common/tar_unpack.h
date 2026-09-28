/*
 * Unpacking a title's data from one tar archive, once, on first start.
 *
 * `pros restore` costs per file, not per byte: a game shipped as its 9,800 files takes
 * over an hour to copy onto the console, where one archive of the same bytes takes
 * seconds. So a title with a large data tree ships it as an uncompressed tar beside
 * `eboot.bin` and unpacks it the first time it starts. Uncompressed, because the point
 * is the file count, the payload needs no decompressor to read it, and the package is
 * compressed as a whole anyway.
 *
 * The archive is POSIX ustar, with GNU's long-name records (`tar --format=gnu`, which
 * is what the packaging writes). Regular files and directories are made; links,
 * devices and every other kind of entry are skipped with a log line, since a data tree
 * has none. A path that would leave the destination (`..`, or absolute) is refused and
 * stops the unpack.
 *
 * Once every entry is written, `marker` is created; its presence is what "already
 * unpacked" means, so an unpack interrupted part-way is simply done again. The archive
 * is left in place.
 */
#ifndef OOPS_APPS_TAR_UNPACK_H
#define OOPS_APPS_TAR_UNPACK_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Called after each file with the number unpacked so far, for a progress display. */
typedef void (*oops_tar_progress_fn)(size_t files_done, void *user);

/*
 * Unpacks `archive` into `dest` unless `marker` exists. 0 when the data is in place
 * (already, or now), -1 when the archive is missing, malformed or could not be written
 * out - each logged.
 */
int oops_tar_unpack_once(const char *archive, const char *dest, const char *marker,
                         oops_tar_progress_fn progress, void *user);

#ifdef __cplusplus
}
#endif

#endif /* OOPS_APPS_TAR_UNPACK_H */
