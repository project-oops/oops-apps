/*
 * Neverball's build generates this from git describe; we pin a commit rather than build
 * from a working tree, so the version is the release that commit is -
 * `neverball-1.6.0`, which `upstream.lock` records as `UPSTREAM_REF`.
 *
 * It is here rather than in `common/` because it is this title's, and because the
 * string is visible: `ball/main.c` puts it in the window title and `--version` prints
 * it.
 */
#ifndef OOPS_NEVERBALL_VERSION_H
#define OOPS_NEVERBALL_VERSION_H

#define VERSION "1.6.0"

#endif
