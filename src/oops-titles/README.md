# Titles

Real programs, ported to run on the console through `oops-sdk`. A probe shows a thing can be
done; a title is the reason for doing it.

**Nothing here may need the user to supply assets or ROMs to build and boot.** An emulator is
the one exception and only a partial one: it builds, boots and runs without any proprietary
code, and that a user must bring their own content afterwards does not stop it being a proof.

## Where each one is

Every target in the survey below now has a directory, a verified pin and its own notes. **A
scaffold is not progress** - it is somewhere for the reading to land, and what separates the rows
is how much of that reading has been done.

| | Runs | Measured | Scaffold only |
|---|---|---|---|
| [neverball](neverball/) | **end to end on hardware** | | |
| [craft](craft/) | | 4/4 shaders compile; 14 of 18 sources compile, `make check` | |
| [supertux](supertux/) | | 29/33 GL entry points, `make check`; renderer read | |
| [extreme-tux-racer](extreme-tux-racer/) | | source list named, not armed | |
| [armagetron-advanced](armagetron-advanced/) | | | pin + notes |
| [supertuxkart](supertuxkart/) | | | pin + notes |
| [retroarch](retroarch/) | | | pin + notes |

Two more directories here are instruments rather than ports: [gl-cts](gl-cts/) runs Khronos'
conformance suite and [mesa-demos](mesa-demos/) is oops-mesa's bring-up.

Each scaffold's `docs/PORTING.md` opens by saying what it inherited and what nobody has checked,
because a directory full of confident prose is how a survey's guesses turn into a project's
assumptions.

## What a title is made of

**A title is an origin, our patches, our shim and its metadata - and nothing else.** None of the
program's own source or assets is committed here. That is the whole shape, and it is the same
for every title:

```
oops-titles/<title>/
  upstream.lock     the origin: where it comes from and exactly which revision
  upstream/         the fetched tree. Never edited, never committed (.gitignore'd)
  patches/          our changes to it, numbered, applied to a clean checkout
  shim/             our code: the entry point, and whatever oops-sdk does not already provide
  sce_sys/          icon0.png, pic0.png, logo.png, param.json
  app.env           the application identifier, name and version
  Makefile          through oops-apps/common/app.mk, like every other app here
```

**Why a lock file rather than a submodule.** A submodule pins a git commit well, needs no code of
ours, and for one title would be the obvious answer. Two things decide it for the set, and
neither is the one that first suggested itself:

- **It would be a third level of submodule.** OOPS holds oops-apps; oops-apps would hold each
  title's upstream. Every bump then costs three commits in three repositories, in a superproject
  whose log is already more than half bumps - 16 of 30 at the time of writing, across 8
  submodules. Eight titles of that is a great deal of churn for something that changes rarely,
  and a gitlink SHA in a diff tells a reviewer nothing that a line of `upstream.lock` does not.
- **The fetch wants shaping.** A 150 MB asset repository wants `--depth 1 --filter=blob:none`;
  `shallow = true` in `.gitmodules` is advisory and unevenly honoured, while a script simply does
  it.

Two arguments that look good and are not: that a non-git origin forces it (Extreme Tux Racer's
upstream is Subversion, but modern git ports exist and are the sensible base anyway), and that a
submodule would burden everyone who clones OOPS (it would not - a plain `git clone` fetches no
submodule content, and one can be initialised on its own). A submodule stays available for an
individual title if one turns out to suit it: the layout above is what matters, not the mechanism
that fills `upstream/`.

**The patch rule: if it can live in the shim, it must.** A patch is for what can only be changed
in the program's own source - a hard-coded include, a `main` that has to become a named entry
point, a path that must move. Anything that can be satisfied by providing a function instead
belongs in `shim/`, because a shim survives the next upstream revision and a patch has to be
rebased. A title whose `patches/` grows faster than its `shim/` is telling us the SDK is missing
something, and that is worth acting on rather than patching around.

**The identity guard, and why this layout helps.** `~/.oops-identity/scan.sh` reads what is
staged. Upstream's tree is never staged, so the scan only ever sees our patches, our shim and our
metadata - which is exactly the surface that could leak, and a small one.

**Application identifiers** follow the same four-letters-and-five-digits shape as everything else
here (`GLCB00001`, `GLPB00001`, `GLUT00001`): four letters naming the title, then `00001`.

**Building and publishing.** The `Makefile` goes through `oops-apps/common/app.mk`, so a title
gets the undefined-symbol check every payload here gets - which matters more for a port than for
anything we wrote, since a program calling a libc function this SDK lacks otherwise links
cleanly and faults on the console. An `oops-apps` workflow builds each title as a native Prospero
payload and publishes it to a release.

## The targets, and what was actually checked

Each entry below was verified by cloning it and reading the source - grepping for `glBegin` and
`glVertexPointer` against `glCreateShader` and `glUseProgram`, checking the shading language
version, and looking for the asset licence in the repository. **Nothing here is from memory**:
GL versions asserted from recollection have already been wrong twice in this project.

**And that method has a blind spot, which SuperTux 2 found.** A title that ships shaders is not
therefore a title that *runs* them: SuperTux carries `#version 100` and `#version 330` files and
also carries a fixed-function backend that loads neither, choosing between them at run time. The
shader files said `gl2/` and `gl3/`; the renderer says `gl1/`. So the grep above answers "what is
in the tree", and the question is "what executes" - which means reading the backend that is
actually selected, not only the assets beside it.

| Slot | Target | Verified | Language |
|---|---|---|---|
| `bring-up/` | mesa-demos | 53 programs under `src/demos/`, purpose-built per GL feature, no assets | C |
| `gl1/` | **Neverball** | vertex arrays and buffer objects, no `glBegin` and no renderer shaders; 184 MB checked out at `neverball-1.6.0`. Its cost is the platform layer - see below | **C**, 106 files |
| `gl1/` | Armagetron Advanced | 23 `glBegin`, no shader calls - but exceptions, RTTI and boost, so **last** rather than second; see the comparison below | C++, 189 files |
| `gl1/` | Extreme Tux Racer | arrays, no `glBegin`, no exceptions or RTTI, SDL **1.2**. Upstream is Subversion; unofficial git mirrors exist and one must be picked | C++, 45 files |
| `gl2/` | **Craft** | `#version 120` - GLSL 1.20 is OpenGL 2.1 - `glCreateShader`/`glUseProgram`, 14 MB with textures | **C** |
| `gl1/` | **SuperTux 2** | **corrected 2026-09-24.** Its `GL20Context` uses no shaders at all - fixed-function `glMatrixMode`/`glEnableClientState`/`glVertexPointer`/`glColor4f`, one texture unit - and the backend is chosen at run time from `glGetString(GL_VERSION)`. `supertux/docs/PORTING.md` | C++, 476 files |
| `gl3/` | SuperTuxKart | its README: "OpenGL >= 3.3 or OpenGL ES >= 3.0" | C++ |
| `gl3/` | SuperTux 2 | the `#version 330` half of the same engine, through `GL33CoreContext` | C++ |
| `multi/` | **RetroArch** | ships `gl1.c`, `gl2.c` **and** `gl3.c` as separate drivers - one app across all three | C |

## The real sorting axis is C versus C++

More than the GL version. Neverball and Craft are C; everything else is substantial C++, which
on a freestanding target needs `-fno-exceptions -fno-rtti` and runtime stubs for `new`, `delete`
and static-initialisation guards. That is a known pattern and a real project, and it is a
**shared** cost - whichever C++ title lands first pays it and the rest follow cheaply.

**With one correction the comparison below forced:** that plan describes a C++ program that does
not use exceptions or RTTI, and not every candidate is one. Extreme Tux Racer is - it is the
right title to pay that cost. Armagetron is not, and needs a runtime with both working plus part
of boost, which is a larger project wearing the same name.

So the order that gets a title running soonest is: **Neverball** (gl1, C), then **Craft** (gl2,
C), then **Extreme Tux Racer** to pay for the C++ runtime once, and Armagetron, SuperTux 2 and
RetroArch after it.

## What the first one costs, read from the source

Measured against `neverball-1.6.0` (`16945b8a`), the revision `neverball/upstream.lock` pins.

**The GL half is not the job.** No `glBegin` at all: it draws through vertex arrays and buffer
objects - 14 `glEnableClientState`, 6 `glVertexPointer`, 14 `glGenBuffers` - which is GL 1.5 and
is measured working in oops-gl. The `glCreateShader` and `glUseProgram` calls a grep turns up
are **not** the renderer; they are in `share/glsl.c` and `share/hmd_common.c`, reached only with
`ENABLE_HMD=openhmd`, which is off by default. The part this SDK exists for is the part already
done.

**The job is the platform and the codecs.** What the payload needs at runtime:

| Needs | What it is for |
|---|---|
| SDL2 | the window and GL context (19 `SDL_GL_*`), events, timing, audio, joystick, threads and mutexes, and `SDL_RWops` |
| SDL2_ttf | `share/font.c` - and freetype under it |
| libpng, libjpeg | `BASE_LIBS` has both unconditionally |
| libvorbisfile | the music |

Around 125 distinct SDL symbols appear across `share/`, `ball/` and `putt/`, though the count
that matters is smaller: a good deal of that is macros and types rather than functions to
implement.

**Four things make it smaller than that table first looks:**

- **`ENABLE_FS=stdio`** is a supported switch, so the virtual filesystem can be plain
  `fopen`/`fread` - which this SDK has - instead of PhysicsFS.
- **`ENABLE_HMD` and `ENABLE_TILT` are off by default**, which removes openhmd, libcwiimote
  *and* every shader call with them.
- **`ENABLE_NLS=0`** drops gettext.
- **`SDL_net` is not a runtime dependency at all.** It appears only in `share/mapc.c`, the map
  compiler - a build-time tool that runs on the host, not code that ships in the payload.

So the shape of the work is: an SDL2 shim over oops-sdk, and four third-party C libraries
vendored the way upstream is. None of it is GL, and none of it is unusual code - which is the
good kind of large.

## The three `gl1` candidates, compared by what they cost

Neverball's shim being thicker than the table implied made the ordering a real question, so the
other two were read the same way. Armagetron from `ArmagetronAd/armagetronad`; Extreme Tux Racer
from `meveric/extremetuxracer`, one of the git mirrors of the Subversion upstream.

| | **Neverball** 1.6.0 | **Extreme Tux Racer** | **Armagetron Advanced** |
|---|---|---|---|
| Language | **C**, 86 `.c` | C++, 45 `.cpp` | C++, 189 `.cpp` |
| Draws with | arrays + buffer objects | arrays (65 `glEnableClientState`) | 23 `glBegin` + arrays |
| Renderer shaders | none (HMD path only) | none | none |
| Exceptions, RTTI | - | **0 `throw`, 0 `dynamic_cast`** | **68 `throw`, 167 `dynamic_cast`** |
| C++ standard library | - | `std::string` (602 uses) | `string` 342, `vector` 157 |
| boost | - | none | **yes** - `shared_ptr`, `variant`, `any`, `lexical_cast` |
| SDL | SDL2 + `_ttf` | **SDL 1.2** + `_image`, `_mixer` | SDL2 + `_image`, `_mixer`, `_syswm` |
| Other C libraries | png, jpeg, vorbisfile | freetype | png |

**The answer is that Neverball stays first, and the reason is sharper than the one in the table
above.** That entry ordered it first for being C and fixed-function, which is true and is not the
point. The point is that *every* candidate needs an SDL shim, so that cost is paid whoever goes
first - and the C++ ones need a C++ runtime **as well**. Going C-first is not a preference for C;
it is declining to pay two new costs in the same title.

**Armagetron moves to last, not second.** The plan in the section below - `-fno-exceptions
-fno-rtti` and stubs for `new`, `delete` and static-initialisation guards - does not describe it:
68 `throw` and 167 `dynamic_cast` mean it *uses* both, so it needs a C++ runtime with exceptions
and RTTI working, plus enough of boost to satisfy `shared_ptr` and `variant`. That is a different
and much larger project than the one that paragraph costs.

**Extreme Tux Racer is the right second**, and is the title that should pay the C++ runtime cost
for the rest. It is C++ used as a better C - classes, but no exceptions, no RTTI, no boost, and
of the standard library essentially `std::string` alone. `-fno-exceptions -fno-rtti` genuinely
applies to it.

> **Both caveats above were resolved by pinning it, and one of them was wrong.** "Its SDL is 1.2"
> is true of `meveric`, the mirror read first, and **not** of `RKSimon`, which is the same
> codebase with the SDL2 port already done. The mirror question is settled in
> `extreme-tux-racer/upstream.lock`, which compares all three - the third, `lutris`, tracks modern
> ETR and moved to SFML, which is a larger dependency than either.
>
> So ETR needs SDL2 and not `oops-deps/sdl12-compat`. That shim was built for this title, links
> with zero duplicate symbols, and remains the general answer for an SDL 1.2 title - it is simply
> not on this one's path. **Read the candidate you are about to pin, not the one you read first.**

**A caution about reading these numbers.** `std::string` first counted 0 in Extreme Tux Racer,
which would have made it look cheaper than it is - four files say `using namespace std;` and
write `string` bare, 602 times. Any grep for a qualified name in C++ has to be checked against
the unqualified one.
