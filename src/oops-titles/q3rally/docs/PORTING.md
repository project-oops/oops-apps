# Porting Q3Rally

How the title is built, and the checks that stand in for things a link cannot see.

| | |
|---|---|
| `make census` | compiles every source for the target and reports what each stops on |
| `make glsurface` | resolves every GL entry point the renderer asks for by name |
| `make qvm` | builds the three game-logic modules and checks `VM_MAGIC_VER2` |
| `make pak` | packs the data and modules into one `pak0.pk3` |
| `make package` | the title directory |

## The source list comes from upstream's build system

ioquake3 assembles its client from `Q3OBJ + Q3ROBJ + JPGOBJ` across a long run of conditionals.
The Makefile's comment carries a `make -n -s targets` invocation that asks the build system for
the list, and the paths are its output. Re-derive it after a bump rather than editing by hand.

**The `USE_*` switches belong in that invocation and nowhere else.** Passing them on the compile
line as `-DUSE_CURL=0` enables curl: every one is tested with `#ifdef`, and upstream only defines
them valueless, when the feature is on. Likewise the code tests `STANDALONE`, not upstream's
Makefile variable `BUILD_STANDALONE`.

**ioquake3 does not call GL directly.** Every entry point goes through a function-pointer table,
so the code says `qglBegin`, and a census of the GL surface counts `qgl[A-Z]*`. A grep for
`gl[A-Z]*` finds ioquake3's own `glConfig`, `glState` and `glIndex`.

## What the port uses from `common/posix`

ioquake3 is a UDP game; `qcommon/net_ip.c` and `sys/sys_unix.c` need:

| | |
|---|---|
| `sys/select.h` | `fd_set` and a real `select`, polled through `MSG_PEEK \| MSG_DONTWAIT` |
| `sys/socket.h` | `bind`, `sendto`, `recvfrom`, `setsockopt`, and the option constants |
| `netinet/in.h` | the IPv6 types, so the IPv4 half compiles - every call behind them fails |
| `netdb.h` | `getaddrinfo`, `getnameinfo`, `gai_strerror` |
| `unistd.h` | `gethostname`, `fork`, `execvp`, `STDIN_FILENO` |
| `signal.h` | `SIGTRAP`, and a `kill` that only answers whether a process exists |

**`select` has to be real.** `NET_Sleep` does not use it to sleep - it asks which sockets have a
packet and hands the result to `NET_GetPacket`, which reads a socket only if `FD_ISSET` says so.
A `select` reporting "nothing ready" would mean no packet is ever received, with no error.

`oops_recvfrom` reports a datagram's sender only if the platform binds `_recvfrom`.

## The by-name GL surface

`sdl_glimp.c:269` fills every `qgl*` pointer from a string:

```c
#define GLE( ret, name, ... ) qgl##name = (name##proc *) SDL_GL_GetProcAddress("gl" #name); \
    if ( qgl##name == NULL ) { ...ERROR: Missing OpenGL function...; success = qfalse; }
```

Core names included - `glBindTexture`, `glBegin`, `glTexCoord2f`. One NULL fails
`GLimp_GetProcAddresses` and the renderer refuses to start. The link cannot see this, because the
functions are in the payload; what finds them at run time is oops-gl's by-name table.
`oops-sdk/src/gl/gl_procs_core.h` carries the core entry points `GL/gl.h` declares, and
`make glsurface` checks every name the renderer asks for against it.

`glGetStringi` is not asked for. It is in `QGL_3_0_PROCS`, bound only by a context claiming GL
3.0, and oops-gl reports 1.1 by default (`gl_state.c:1844`).

## The game logic

ioquake3 is an engine. The rules, the HUD and the menus are three modules loaded at run time, and
`upstream/baseq3r` ships art and configuration with no compiled game logic - no `.qvm`, no
`.pk3`, no shared library. Without `make qvm` the title starts, opens the display and stops in
`VM_Create`.

A `.qvm` is bytecode for ioquake3's own virtual machine and is byte-identical whatever machine
builds it, so `make qvm` builds upstream's `lcc` and `q3asm` as host binaries and runs them. The
engine interprets the result: `vm_interpreted.c` is in the source list and `NO_VM_COMPILED` is
set, because the alternative writes and executes machine code at run time and this target has no
memory that is both writable and executable.

`vm_game`, `vm_cgame` and `vm_ui` are set to 2 - the interpreter - by `shim/q3_start.c`, which is
what upstream's own `run.sh` passes.

## The data package

`pros restore` costs per file, so `make pak` builds one `pak0.pk3` - a zip, which
`qcommon/unzip.c` reads natively - holding the data files and the three modules under `vm/`.

**The paths inside are relative to the game directory.** An archive rooted at `baseq3r/` is one
the engine opens, finds nothing in, and reports as an empty pk3, so `scripts/pak.sh` refuses to
write one.

## Finding the data

`shim/q3_start.c` probes both `/app0` and `/data/homebrew/<id>`, accepts either the packed or the
loose layout, logs which it found, and refuses to start when neither is there - the engine's own
failure for a wrong root is `Couldn't load default.cfg`, which names a file when the problem is
the directory above it.

Sound opens an SDL audio device through `snd_dma.c`; the Vorbis, Opus and Theora codecs are
switched off.
