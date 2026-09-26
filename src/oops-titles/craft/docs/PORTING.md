# Porting Craft

What the shim replaces, how each platform dependency is answered, and what the build asks of
oops-gl. `make census` reports which sources compile.

## What the shim replaces

| Upstream wants | This port gives it | Why not port the real thing |
|---|---|---|
| GLFW 3 | `shim/glfw_shim.c` | GLFW abstracts a window system; this platform has one display that is already open. A GLFW backend would answer "one window, no chrome, no resize" to everything |
| GLEW | `shim/include/GL/glew.h` | GLEW finds GL entry points at run time because a desktop's GL is whatever is installed. oops-gl is linked in: a missing function is a link error naming it, rather than a null pointer called in frame one |
| libcurl (2 calls in `main.c`) | `shim/include/curl/curl.h` | Two no-op globals. The file that uses curl is excluded |
| pthreads | `shim/include/pthread.h` | oops-sdk has threads, mutexes and condition variables with the same lifetimes. The mapping is one call to one call |

**The GLFW surface is measured, not chosen.** It is every `glfw*` and `GLFW_*` token in Craft's
own sources; a function missing from the shim is one Craft does not call.

## Platform dependencies

### Sockets - `client.c`

`client.c` includes `<netdb.h>` and is Craft's multiplayer client. oops-sdk has BSD-style
sockets (`oops/net.h`: `oops_socket`, `oops_connect`, `oops_recv`), so the answer is a header shim
over an API that exists, the same shape as `pthread.h` next to it. `client.c` is inert until
`client_enable()` is called, and single-player Craft never calls it.

### SQLite's platform layer - `sqlite3.c`

- **`<fcntl.h>` and a VFS.** SQLite's unix backend opens files with `open`/`fcntl` and locks them
  with POSIX record locks. oops-sdk has a filesystem (`oops/fs.h`) and no advisory locking, which
  SQLite wants for multi-process access that cannot happen here. The answer is a custom VFS
  against `oops/fs.h` - SQLite is designed for this - with no patch to SQLite.
- **`localtime` and `struct tm`.** SQLite's date functions want them, and oops-sdk's `<time.h>`
  omits them so a program that formats a date with this clock fails to link rather than printing
  1970. Craft stores integers and never formats a date, so `SQLITE_OMIT_DATETIME_FUNCS` is the
  switch rather than a fake `localtime`.

### Threads - `tinycthread.c`

`tinycthread.c` includes `<signal.h>`, answered by a one-line shim header.

### Host headers

`common/app.mk` passes `-nostdlibinc` on the freestanding side, so a missing header is reported
by its own name rather than by a glibc internal (`bits/wordsize.h`) reached through the build
machine's `/usr/include`. A hosted title does not get the flag, because inside its sysroot it
would remove the C library the title is meant to use.

## Multiplayer, and why `auth.c` is not compiled

`auth.c` is the only file that includes `<curl/curl.h>`. It POSTs a username and an identity
token to a server over HTTPS. Porting that means porting libcurl and a TLS stack, for a feature
this port does not have.

A libcurl shim whose `curl_easy_perform` fails would offer online play, attempt it, and fail in a
dialog that explains nothing. **A login that always fails is a bug report; a login that is not
there is a scope.** So the file is excluded and `shim/craft_offline.c` supplies the one symbol
`main.c` calls, returning upstream's own "no token", which `main.c` handles by carrying on with
the local world.

## The shaders

oops-gl compiles a GL 2.0 fragment shader to gfx1030 instructions or refuses it by name, and a
refusal is not a fallback: the draw fails with `GL_INVALID_OPERATION`. `make check` compiles
Craft's four fragment shaders from `upstream/shaders/` and reports each against the limits.
`block` uses both texture sets, which is all of them, and most of the uniform floats a draw
carries; one more sampler and it does not compile.

The back end supports two things these shaders use:

- **`int` and `bool` uniforms.** `block_fragment.glsl` has `uniform int ortho` and
  `text_fragment.glsl` has `uniform bool is_sign`. A program's value pool stores every uniform as
  a float, so these are carried as floats; integer arithmetic on them is refused.
- **The `bool()` constructor.** `bool(ortho)` is `x != 0`, not a component copy - through the
  ordinary constructor path `bool(2.0)` would equal 2.0, which is true in a condition and wrong
  everywhere the value is read.

The vertex shaders are not compiled for the hardware: oops-gl's hardware vertex stage is a
passthrough, and a GL 2.0 vertex shader runs on the CPU in the reference interpreter, which has
the whole built-in library. `block_vertex.glsl`'s `atan`, `normalize`, `pow` and `distance` are
all available there, while `atan` and `refract` are refused by the fragment back end.

## Upstream's warnings

`common/app.mk` compiles at `-Werror` with `-Wconversion`, `-Wsign-conversion` and
`-Wstrict-prototypes`. Craft was not written for that: it has missing prototypes, implicit float
conversions and `/*` inside block comments. Those warnings are switched off in the Makefile
rather than patched away - a patch to somebody else's C is a rebase burden for no behaviour
change.
