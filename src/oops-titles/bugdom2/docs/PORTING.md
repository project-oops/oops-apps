# Porting Bugdom 2

What is measured, what is decided, and what has not happened. Every number here came from a build.

## Where it stands

| | |
|---|---|
| C sources compiling for the target | **97 of 97** — `make census`, first run, no blockers |
| C++ sources compiling | **23 of 23** — 22 Pomme, 1 upstream (`Boot.cpp`); the entry-point shim makes 24 in the archive |
| SDL | **SDL2**, read at the pinned revision |
| GL entry points | **71**, all covered by oops-gl. **Two are looked up by name** — see below |
| Game data | ships with upstream, `Data/`, **404 files** — no player purchase, no archive |
| Linked | **yes.** `build/bugdom2.elf`, 5,615,576 bytes, `app.mk`'s undefined-symbol guard clean |
| Packaged | **yes.** `make package` — eboot 4,905,408 bytes, `sce_sys/`, `sce_module/libc.prx`, `Data/`; 413 files / 185 MB |
| Run on hardware | **no** |

The sizes move between builds because the payload links the SDK tree as it stands, and other work
lands in it continuously. Count the link, not the byte count.

## Two faults a clean link does not find

Both apply to any title linking the full `libc++.a`, and both are written out in
[`../../bugdom/docs/PORTING.md`](../../bugdom/docs/PORTING.md).

- **`-DOOPS_CXX_EXTERNAL_NEW_DELETE`**, because `libc++.a`'s `operator new` carries a guard asserting
  its own address is inside `__lcxx_override`, and `common/cxxrt.cpp`'s definition — which wins under
  `--allow-multiple-definition` — is not. Without the define the first libc++ allocation is a
  privileged-instruction fault. OOPSy-daisy found it on hardware; the fix is confirmed there.
- **`oops_run_init_array()`** in the entry point, because `.init_array` is not walked for a plain C++
  payload, so namespace-scope constructors never execute. It fails quietly: `.bss` is already zero, so
  nothing breaks until a constructor stores a non-zero value.

## It cost almost nothing, and that is the finding

Bugdom 2 is the same author, the same engine lineage, the same Pomme, the same SDL2 and the same
freeware-data arrangement as Bugdom. Every expensive question had already been answered next door:

- whether libc++'s `<filesystem>` could replace Pomme's bundled `ghc::filesystem` — it can, and the
  reasoning is in [`../../bugdom/docs/PORTING.md`](../../bugdom/docs/PORTING.md);
- what the POSIX layer was missing — eleven declarations, all shared, all already committed;
- why `main` comes out mangled in a freestanding C++ translation unit, and why the entry-point shim
  therefore has to be C++;
- why `OOPS_CXX_EXCEPTIONS` has to be set *above* the libc++ include.

So this title's whole cost was a lock, a Makefile, an entry point, an icon and **the same one-line
patch**, and its first census came back 97 of 97 and 23 of 23 with no blockers at all. That is worth
recording: the second title in a family is nearly free, and the first one is where the platform work
lives.

## Where it differs from Bugdom

**`POMME_NO_QD3D`** (`CMakeLists.txt:57`). Bugdom draws through Pomme's QuickDraw 3D
reimplementation; Bugdom 2 brought its own `Source/3D` and switches Pomme's off. So three more Pomme
sources are out of the build — 22 here against Bugdom's 25 — the `-I.../QD3D` include Bugdom needs is
absent, and the model format is `.bg3d` rather than `.3dmf`. Checked rather than assumed: no source
here includes `QD3D.h` by bare name, which is what made that include necessary in Bugdom.

**A different Pomme revision.** This commit's tree records `c6a38eab`; Bugdom 1.3.4's records
`ef94150e`. That is upstream's choice, and one pinned hash still pins everything because the submodule
revision lives in the pinned commit's own tree.

**It uses a GL loader.** `Source/3D/OGL_Support.c:132,135` binds `glActiveTexture` and
`glClientActiveTexture` through `SDL_GL_GetProcAddress`. Bugdom names no loader at all, so a missing
entry point there is an ordinary link error; here a lookup that answers NULL is a null call at run
time and nothing else catches it. Both names are in oops-gl's `gl_procs.h` table.

`make glsurface` is the standing check. It reads the names **out of the source** rather than from a
list in the Makefile, so a name added upstream is checked too, and it **asserts the count is
non-zero** — a grep that silently matched nothing would otherwise report a clean surface, which is
the failure mode q3rally's equivalent check was written to avoid.

**`gluLookAt` and `gluPerspective`**, so `OOPS_FEATURES` names `glu`. Bugdom's does not.

## The GL surface, and three names that are not calls

71 entry points, every one defined by oops-gl. A grep for `gl[A-Z]*` finds 74, and three of them are
not calls:

| | |
|---|---|
| `glLockArraysEXT` | commented out, `Source/3D/MetaObjects.c:756` |
| `glUnlockArraysEXT` | commented out, `Source/3D/MetaObjects.c:759` |
| `glTextureName` | a local variable, `Source/3D/OGL_Support.c:848` |

Worth checking rather than trusting the count: two of the three would have read as a missing
compiled-vertex-array extension, and `EXT_compiled_vertex_array` is not something oops-gl has.

## The entry point

`shim/bugdom2_start.cpp`. `FindGameData` (`Source/Boot.cpp:30`) derives the data directory from
`argv[0]` — on a non-Apple build its first attempt is `parent_path(argv[0]) / "Data"` — and accepts the
result only when `Data/Skeletons/Grasshopper.bg3d` opens. So the shim probes for **that one file**
under `/data/homebrew/BGII00001` and then `/app0`, and sets `argv[0]` to the root that answered.
Probing for `Data/` alone would pass on a package whose assets failed to copy.

It refuses to start when neither root answers, rather than letting `FindGameData` fall through to its
later attempts: those end in `throw std::runtime_error("Couldn't find the Data folder.")`, which
surfaces as a message box that names no path and needs a controller to dismiss.

It also exports **`HOME`**, because Pomme's `FindFolder` reads `XDG_CONFIG_HOME` then `HOME` and
returns `fnfErr` when both are unset — another alert box. The environment starts empty on this
platform, so the one name a payload genuinely knows is set here, and `OOPS_POSIX_HOME/.config` is
created for it.

It is **C++**, and has to be: in a freestanding C++ translation unit `main` is an ordinary function, so
`Boot.cpp` exports `_Z4mainiPPc`. A C shim asking for `main` links, because a payload link ignores
unresolved symbols, and would fault at the first instruction. Bugdom's shim was written in C first and
`common/app.mk`'s guard is the only reason that did not reach hardware.

## The link

`make` produces `build/bugdom2.elf`, and `common/app.mk`'s undefined-symbol guard is the gate: it runs
`nm -u` on the payload before anything else sees it and fails the build on any non-weak name nothing
defines. That check is the one that matters, because the payload link itself is
`--unresolved-symbols=ignore-all`.

**After `mkmodule` it cannot be used.** The packaged ELF is a Sony module with unmapped
`DT_SCE_*` dynlibdata, and `nm` on it reports zero undefined symbols whether or not any exist. Read
the guard's output from the link, never `nm` on the artefact.

## What has not been measured

- **Nothing has run.** No frame, no sound, no input, no controller mapping.
- The GL surface is covered by name, by the loader check and by the link — not by a draw.
- `oops-deps/sdl2`'s joystick driver sends both Back and Guide from `OOPS_BUTTON_CREATE`
  (`OOPS_BUTTON_CREATE` and `OOPS_BUTTON_PS` are the same bit). Whether that matters here is unknown
  until it runs.
