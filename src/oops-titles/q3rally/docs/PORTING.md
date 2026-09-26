# Porting Q3Rally

What is built, what each number came from, and the one thing that has not happened yet. Every
figure here is from a build or a file on disk; none is an estimate.

## Where it stands

| | |
|---|---|
| Sources compiling for the target | **132 of 132** — `make census` |
| Payload | `build/q3rally.elf`, **4.97 MB**, linked |
| Undefined symbols | **none** outside the platform's own imports — `app.mk`'s guard passes |
| GL entry points resolved by name | **66 asked for, 66 answered** — `make glsurface` |
| Game logic | **3 QVM modules**, `VM_MAGIC_VER2` checked — `make qvm` |
| Data | **one `pak0.pk3`**, 198 MB, 3,625 entries — `make pak` |
| Package | **10 files, 203 MB** — `make package` |
| Run on hardware | **not yet.** Nothing below has been observed on a console |

## The source list came from upstream's build system

ioquake3 assembles its client from `Q3OBJ + Q3ROBJ + JPGOBJ` across 3,420 lines of conditionals. A
hand-written list would be a guess wearing the shape of a fact, so the Makefile's comment carries a
`make -n -s targets` invocation that asks the build system instead, and the 132 paths are its
output. Re-derive it after a bump rather than editing by hand.

**The `USE_*` switches belong in that invocation and nowhere else.** Passing them on the compile
line as `-DUSE_CURL=0` *enables* curl: every one is tested with `#ifdef` — 42 sites for `USE_CURL`,
53 for `USE_VOIP` — and upstream only ever defines them valueless, when the feature is on. That cost
26 sources, all failing on `osreldate.h` reached from the HTTP downloader the build had been told to
leave out. The same shape: the code tests `STANDALONE`, not upstream's Makefile variable
`BUILD_STANDALONE`.

## The census, and the mistake it nearly recorded

A first pass grepped for `gl[A-Z]*` and found 23 names, 11 "missing". Every one of those 11 was
ioquake3's own — `glConfig`, `glState`, `glIndex`, and a typo of their own making, `glDrawElemet`.
**ioquake3 does not call GL directly**: every entry point goes through a function-pointer table, so
the code says `qglBegin`. Censusing `qgl[A-Z]*` gives the real surface.

That was the right correction and it was not the end of the story — see below.

## What the port needed from `common/posix`

The shim had a TCP client. ioquake3 is a UDP game, and `qcommon/net_ip.c` and `sys/sys_unix.c` were
the last two sources not to compile. Added, all of it reusable and none of it in this title:

| | |
|---|---|
| `sys/select.h` | `fd_set` and a **real** `select`, polled through `MSG_PEEK \| MSG_DONTWAIT` |
| `sys/socket.h` | `bind`, `sendto`, `recvfrom`, `setsockopt`, and the option constants |
| `netinet/in.h` | the IPv6 types, so the IPv4 half compiles — every call behind them fails |
| `netdb.h` | `getaddrinfo`, `getnameinfo`, `gai_strerror` |
| `unistd.h` | `gethostname`, `fork`, `execvp`, `STDIN_FILENO` |
| `signal.h` | `SIGTRAP`, and a `kill` that only answers whether a process exists |

**`select` is the one that had to be real.** `NET_Sleep` does not use it to sleep — it asks which
sockets have a packet and hands the result to `NET_GetPacket`, which reads a socket only if
`FD_ISSET` says so. A `select` reporting "nothing ready" would not slow the game down; it would mean
no packet is ever received, with no error anywhere.

## The by-name GL surface, which the link cannot tell you about

**This is the one that would have cost a console run.** `sdl_glimp.c:269` fills every `qgl*` pointer
from a *string*:

```c
#define GLE( ret, name, ... ) qgl##name = (name##proc *) SDL_GL_GetProcAddress("gl" #name); \
    if ( qgl##name == NULL ) { ...ERROR: Missing OpenGL function...; success = qfalse; }
```

Core names included — `glBindTexture`, `glBegin`, `glTexCoord2f`. One NULL fails
`GLimp_GetProcAddresses` and the renderer refuses to start. So the payload links with nothing
undefined, because all 66 of those functions *are* in it, and still would not draw: what finds them
at run time is `oops-gl`'s by-name table, and that table held extensions only, on the reasoning that
core GL is linked by symbol and never looked up. True of Neverball. Not true here.

Measured: **66 asked for, 0 answered.** `oops-sdk/src/gl/gl_procs_core.h` now carries the 485 core
entry points `GL/gl.h` declares and the always-linked GL objects define, and `make glsurface` is the
check — it answered 0 before and 66 after, so it can fail as well as pass.

`glGetStringi` is **not** asked for. It is in `QGL_3_0_PROCS`, bound only by a context claiming GL
3.0, and `oops-gl` reports 1.1 by default (`gl_state.c:1844`). The earlier note calling it "the one
real gap" was right about the mechanism and looking at the wrong list.

## The game logic is not in the payload

ioquake3 is an engine. The rules, the HUD and the menus are three modules loaded at run time, and
`upstream/baseq3r` ships 3,623 files of art and configuration with **no compiled game logic at
all** — no `.qvm`, no `.pk3`, no shared library. Without `make qvm` the title starts, opens the
display and dies in `VM_Create`.

A `.qvm` is bytecode for ioquake3's own virtual machine and is byte-identical whatever machine builds
it, so `make qvm` builds upstream's `lcc` and `q3asm` as **host** binaries and runs them. The engine
interprets the result: `vm_interpreted.c` is in the source list and `NO_VM_COMPILED` is set, because
the alternative writes and executes machine code at run time and this target has no memory that is
both writable and executable.

`vm_game`, `vm_cgame` and `vm_ui` are set to 2 — the interpreter — by `shim/q3_start.c`, which is
what upstream's own `run.sh` passes.

## One file instead of 3,623

`pros restore` costs per file: 1,775 files measured at 81 minutes. The loose tree would be the better
part of an afternoon every time the data moved. `make pak` builds one `pak0.pk3` — a zip, which
`qcommon/unzip.c` reads natively — holding all 3,622 data files and the three modules under `vm/`.
607 MB becomes 198 MB, and the package is 10 files.

**The paths inside are relative to the game directory.** An archive rooted at `baseq3r/` is one the
engine opens successfully, finds nothing in, and reports as an empty pk3, so `scripts/pak.sh`
refuses to write one.

## Finding the data, by looking rather than deciding

Two titles here disagree about `/app0` and both are right about themselves: Neverball reads it and
works, Craft measured that a homebrew title cannot open it at all and lives at
`/data/homebrew/<id>`. `shim/q3_start.c` probes both, accepts either the packed or the loose layout,
logs which it found, and **refuses to start** when neither is there — because the engine's own
failure for a wrong root is `Couldn't load default.cfg`, which names a file when the problem is the
directory above it.

## What has not been measured

- **It has never run.** Everything above is a build or a file. No frame, no input, no sound and no
  network traffic has been observed on a console.
- **Sound.** `snd_dma.c` opens an SDL audio device on the way up; whether it produces anything is
  unmeasured, and the codecs (Vorbis, Opus, Theora) are switched off.
- **Networking.** The shim compiles and `select` is real, but no packet has crossed it here.
  `oops_recvfrom` reports a datagram's sender only if the platform binds `_recvfrom`; obSCEne's
  hardware run says it does, and that has not been exercised.
- **Bots.** `botlib` is compiled and its files are in the pk3; nothing has run a match.
