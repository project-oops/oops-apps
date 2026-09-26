/*
 * `sys/mman.h` - the constants, and deliberately **no `mmap`**.
 *
 * ioquake3's `sys_unix.c:34` includes this and uses nothing from it, which is the common case: a
 * Unix source includes the header its platform block conventionally includes. So the header has to
 * exist, and that is all it has to do.
 *
 * **`mmap` and `munmap` are not declared, and that is the point.** There is no memory mapping on
 * this platform. Declaring them would let a real caller compile and then fail at link - and a
 * payload link does not report an unresolved symbol, so it would become a jump into nothing on the
 * console. Leaving them undeclared makes a genuine user of `mmap` fail *here*, at compile time,
 * with the function's own name in the error. That is the loudest failure available.
 *
 * A port that actually needs mapped memory wants `oops_mem_alloc` from `<oops/memory.h>`, which is
 * real, or `<oops/jit.h>` if the mapping has to be executable.
 *
 * The constants are FreeBSD's values, because that is the kernel underneath and a program that
 * stores or compares them should see the platform's own numbers.
 */
#ifndef OOPS_POSIX_SYS_MMAN_H
#define OOPS_POSIX_SYS_MMAN_H

#define PROT_NONE  0x00
#define PROT_READ  0x01
#define PROT_WRITE 0x02
#define PROT_EXEC  0x04

#define MAP_SHARED  0x0001
#define MAP_PRIVATE 0x0002
#define MAP_FIXED   0x0010
#define MAP_ANON    0x1000
#define MAP_ANONYMOUS MAP_ANON

#define MAP_FAILED ((void *)-1)

#define MS_SYNC       0x0000
#define MS_ASYNC      0x0001
#define MS_INVALIDATE 0x0002

#endif /* OOPS_POSIX_SYS_MMAN_H */
