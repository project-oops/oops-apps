/*
 * `sys/param.h`, which BSD code includes for its path-length limit. PhysFS's
 * `physfs_platform_unix.c` is the first here, on the `__FreeBSD__` branch clang selects for this
 * target.
 *
 * 1024 is FreeBSD's `MAXPATHLEN`, and `PATH_MAX` is the same number there.
 */
#ifndef OOPS_POSIX_SYS_PARAM_H
#define OOPS_POSIX_SYS_PARAM_H

/*
 * **`BSD` belongs here, and its absence was an omission rather than a decision.**
 *
 * On a real BSD this header is where the `BSD` manifest constant is defined, and that is the
 * *only* place it comes from - which is why portable code includes `<sys/param.h>` purely to test
 * it. Pomme's bundled `ghc::filesystem` says so in as many words: `#include <sys/param.h>`, with
 * the comment "define BSD manifest constant only in sys/param.h", and then an OS chain ending at
 * `#error "Operating system currently not supported!"`. Eight of Bugdom's C++ sources stopped
 * there, and the reason was that this file answered the include and not the question.
 *
 * `199506` is FreeBSD's value - 4.4BSD-Lite2, which is the lineage this kernel is from. A program
 * comparing against `199306` or `199506` to gate an interface gets the same answer it would on the
 * machine underneath.
 */
#ifndef BSD
#define BSD 199506
#endif

#ifndef MAXPATHLEN
#define MAXPATHLEN 1024
#endif
#ifndef PATH_MAX
#define PATH_MAX MAXPATHLEN
#endif

#endif /* OOPS_POSIX_SYS_PARAM_H */
