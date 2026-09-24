# Porting Craft

What is done, what is left, and what each remaining gap actually is. The numbers are measured -
every one of them came from a build, not an estimate.

## Where it stands

**Every source compiles except three, and the three name three headers between two subsystems
that matter and one that does not.**

**These counts come from `make census`**, which compiles every source under the real target flags
and prints what each one died on. They were prose before, and prose drifts: this table said 11 of
12 of Craft's own sources compiled, counting `auth.c` as one of them when it does not and is not
meant to, and listed `tinycthread` among the vendored libraries that build while naming the
`signal.h` it needs four paragraphs further down. Both were written in good faith and both were
wrong by the time anybody read them.

| | |
|---|---|
| Craft's own sources | **10 of 12 compile.** `client.c` wants `netdb.h` - see *sockets* below - and `auth.c` does not compile either, deliberately: see *Multiplayer* |
| Vendored libraries | **2 of 4 compile**: `lodepng` and `noise`. `sqlite3.c` wants `fcntl.h` and `tinycthread.c` wants `signal.h` |
| The shim | 2 sources, both compile |
| **The shaders** | **4 of 4 pairs compile for the console** - `make check` |

Counting the way the prose below does - `auth.c` excluded by choice rather than failed - that is
**three** sources short of a complete build, and they are the three the rest of this document is
about.

`auth.c` is excluded rather than counted as a failure; *Multiplayer* below says why.

## What the shim replaces

| Upstream wants | This port gives it | Why not port the real thing |
|---|---|---|
| GLFW 3 | `shim/glfw_shim.c`, 23 entry points | GLFW abstracts a window system; this console has one display that is already open. A GLFW *backend* would answer "one window, no chrome, no resize" to everything |
| GLEW | `shim/include/GL/glew.h` | GLEW finds GL entry points at run time because a desktop's GL is whatever is installed. oops-gl is linked in: a missing function is a link error naming it, which is strictly better than a null pointer called in frame one |
| libcurl (2 calls in `main.c`) | `shim/include/curl/curl.h` | Two no-op globals. The file that *uses* curl is excluded |
| pthreads | `shim/include/pthread.h` | oops-sdk already has threads, mutexes and condition variables with the same lifetimes. The mapping is one call to one call |

**The GLFW surface is measured, not chosen.** It is every `glfw*` and `GLFW_*` token in Craft's
own sources; a function missing from the shim is one Craft does not call.

## The two gaps

### 1. Sockets — `client.c`

`client.c` includes `<netdb.h>` and is Craft's multiplayer client. oops-sdk has BSD-style
sockets (`oops/net.h`: `oops_socket`, `oops_connect`, `oops_recv`), so this is a header shim over
an API that exists rather than a port - the same shape as `pthread.h` next to it.

**It is a gap and not a blocker.** `client.c` is inert until `client_enable()` is called, and a
single-player Craft never calls it. The alternative to shimming it is excluding the file and
stubbing the fourteen `client_*` functions `main.c` calls, which is a larger stub than the shim.

### 2. SQLite's platform layer — `sqlite3.c`

Two separate things, and only the first is real work:

- **`<fcntl.h>` and a VFS.** SQLite's unix backend opens files with `open`/`fcntl` and locks them
  with POSIX record locks. oops-sdk has a filesystem (`oops/fs.h`); what it does not have is
  advisory locking, and SQLite wants it for multi-process access that cannot happen here. The
  usual answer is a custom VFS - SQLite is designed for exactly this - which is a few hundred
  lines against `oops/fs.h` and no patch to SQLite at all.
- **`localtime` and `struct tm`.** SQLite's date functions want them, and oops-sdk's `<time.h>`
  **deliberately does not have them**: its own header says a port that formats a date with this
  clock "will print nonsense, so it should not", and omits them so such a program fails to link
  rather than printing 1970. Craft stores integers and never formats a date, so
  `SQLITE_OMIT_DATETIME_FUNCS` is the honest switch rather than a fake `localtime`.

`tinycthread.c`'s `<signal.h>` is a third missing header and is one line of shim; it is listed
here as work rather than as a footnote, because until it is written `tinycthread.c` does not
compile - which is what `make census` says and what the table above used to deny.

## The host headers, which is not only this title's problem

**`common/app.mk`'s `TARGET_CFLAGS` carries `-nostdlib` and not `-nostdlibinc`.** The first is a
linker flag. The header search path is left alone, so a freestanding target compile falls through
to the build machine's `/usr/include`.

This title is where it shows, because this title is the one that asks for headers the SDK does
not have. Measured both ways, the same three files:

| | without `-nostdlibinc` | with it |
|---|---|---|
| `client.c` | `'bits/wordsize.h' file not found` | `'netdb.h' file not found` |
| `sqlite3.c` | `'bits/wordsize.h' file not found` | `'fcntl.h' file not found` |
| `tinycthread.c` | `'bits/wordsize.h' file not found` | `'signal.h' file not found` |

`bits/wordsize.h` is a glibc internal. It is reached because clang had already *accepted* glibc's
`netdb.h`, `fcntl.h` and `signal.h` and was following them inward - so the error names a file
nobody wrote, three levels below the one that is actually missing.

**The confusing error is the mild version.** The case with no error at all is the problem: a
source whose headers glibc happens to satisfy compiles cleanly against declarations this target
will never link, and nothing anywhere says so. A freestanding build that can silently reach the
host's libc headers is not freestanding; it is a build that happens to agree with its host.

The flag is set in this title's `Makefile` rather than in `common/app.mk`, because that file is
every title's flag set and this is one title's measurement. Neverball reached the same conclusion
independently and put `-nostdlibinc` in its own `OOPS_NB_CFLAGS` for upstream's archive - but not
on the shim files beside it, which still compile with the host's headers in reach.

**Two titles have now worked around this separately, which is the argument for fixing it once.**
It is not done here: changing `TARGET_CFLAGS` changes every title in the collection at once,
including ones other people are mid-way through, and it wants its own measurement of what breaks
rather than riding along with a documentation correction.

## Multiplayer, and why `auth.c` is not compiled

`auth.c` is the only file that includes `<curl/curl.h>`. It POSTs a username and an identity
token to a server over HTTPS. Porting that means porting libcurl **and** a TLS stack, for a
feature this port does not have.

The alternative - a libcurl shim whose `curl_easy_perform` fails - is worse in a specific way:
the game would offer online play, attempt it, and fail in a dialog that explains nothing. **A
login that always fails is a bug report; a login that is not there is a scope.** So the file is
excluded and `shim/craft_offline.c` supplies the one symbol `main.c` calls, returning upstream's
own "no token", which `main.c` already handles by carrying on with the local world.

## The shaders, which are the part that was in doubt

oops-gl compiles a GL 2.0 fragment shader to real gfx1030 instructions or **refuses it by name**,
and a refusal is not a fallback: the draw fails with `GL_INVALID_OPERATION`. So "do Craft's
shaders compile" was a real question about whether this title can render at all, and
`make check` answers it against `upstream/shaders/` directly:

```
  block    pass  224 words   56/136 regs   7/16 varying  25/32 uniform  2/2 tex
  line     pass   22 words   20/136 regs   0/16 varying  16/32 uniform  0/2 tex
  sky      pass   36 words   21/136 regs   2/16 varying  18/32 uniform  1/2 tex
  text     pass   71 words   29/136 regs   2/16 varying  18/32 uniform  1/2 tex
```

**`block` uses both texture sets, which is all of them**, and 25 of the 32 uniform floats a draw
carries. It is the shader to watch: one more sampler and it does not compile.

Getting there needed two things from the back end, both found by compiling these files rather
than by reading them:

- **`int` and `bool` uniforms.** `block_fragment.glsl` has `uniform int ortho` and
  `text_fragment.glsl` has `uniform bool is_sign`. A program's value pool already stores every
  uniform as a float, so these are carried as the floats they are; integer *arithmetic* on them
  is still refused.
- **The `bool()` constructor.** `bool(ortho)` is `x != 0`, not a component copy - running it
  through the ordinary constructor path would make `bool(2.0)` equal 2.0, which is true in a
  condition and wrong everywhere the value is read.

The vertex shaders are not compiled: oops-gl's hardware vertex stage is a passthrough and a GL
2.0 vertex shader runs on the CPU in the reference interpreter, which has the whole built-in
library. `block_vertex.glsl`'s `atan`, `normalize`, `pow` and `distance` are all fine there, and
two of them (`atan`, and `refract`) are refused by the *fragment* back end - which is why it
matters that the split is what it is.

## Upstream's warnings

`common/app.mk` compiles at `-Werror` with `-Wconversion`, `-Wsign-conversion` and
`-Wstrict-prototypes`. Craft was not written for that: the first build produced 33 missing
prototypes, 44 implicit float conversions and a dozen `/*` inside block comments. All of them
are switched off in the Makefile rather than patched away - a patch to 6,000 lines of somebody
else's C is a rebase burden for no behaviour change, and a warning upstream has lived with for a
decade on four compilers is not news.
