/*
 * SuperTux's `version.h`, which upstream's CMake generates from `version.h.in` using
 * `git describe`. The fetched tree is a detached checkout of the tag `upstream.lock`
 * pins, so the answer is fixed - and a build that asked git would get the port's
 * repository, not upstream's.
 */
#ifndef VERSION_H
#define VERSION_H

#define PACKAGE_VERSION "0.6.3"
#define SUPERTUX_BUILD_NUMBER "c1ddb4f2"

#endif /* VERSION_H */
