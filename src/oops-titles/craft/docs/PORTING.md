# Porting Craft

What is done, what is left, and what each remaining gap actually is. The numbers are measured -
every one of them came from a build, not an estimate.

## Where it stands

**Every source compiles except three, and the three name two subsystems.**

| | |
|---|---|
| Craft's own sources | 11 of 12 compile. The twelfth is `client.c` - see *sockets* below |
| Vendored libraries | 3 of 4 compile: `lodepng`, `noise`, `tinycthread`. The fourth is SQLite |
| The shim | 4 files, all compile |
| **The shaders** | **4 of 4 pairs compile for the console** - `make check` |

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
here for completeness rather than as work.

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
