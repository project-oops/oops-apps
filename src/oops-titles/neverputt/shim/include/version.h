/*
 * Neverball's build generates this from git describe; we pin a commit rather than build
 * from a working tree, so the version is the release that commit is -
 * `neverball-1.6.0`, which `upstream.lock` records as `UPSTREAM_REF`.
 *
 * The version is the *tree's*, not the binary's: Neverputt ships out of the Neverball
 * release and has no number of its own. `putt/main.c` puts this in the window title and
 * `--version` prints it.
 */
#ifndef OOPS_NEVERPUTT_VERSION_H
#define OOPS_NEVERPUTT_VERSION_H

#define VERSION "1.6.0"

#endif
