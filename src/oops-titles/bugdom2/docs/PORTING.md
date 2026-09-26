# Porting Bugdom 2

How this port is put together. It shares its shape with Bugdom, so
[`../../bugdom/docs/PORTING.md`](../../bugdom/docs/PORTING.md) holds the reasoning that applies to
both: the SDL2 pin, Pomme's transcribed source list, libc++'s `<filesystem>` in place of the bundled
`ghc::filesystem`, and the four C++ constraints a title on this platform meets. Only the differences
are here.

## Shape

A 2002 Macintosh game ported to SDL2 and immediate-mode OpenGL by jorio, on the same Pomme.
`OOPS_RENDERER` is `gl1`. The game data ships upstream under `Data/`, and `make package` copies it
beside the eboot, failing if `Data/Skeletons/Grasshopper.bg3d` is absent from the result.

`upstream.lock` pins upstream's only release tag, which asks for SDL2 at `CMakeLists.txt:68`.

## Pomme without QuickDraw 3D

`CMakeLists.txt:57` sets `POMME_NO_QD3D` where Bugdom does not: this game brought its own
`Source/3D`. Three Pomme sources drop out, the `-I.../QD3D` include Bugdom needs is absent, and the
model format is `.bg3d` rather than `.3dmf`. No source here includes `QD3D.h` by bare name, which is
what made that include necessary in Bugdom.

The pinned tree records a different Pomme revision from Bugdom's. That is upstream's choice, and one
pinned hash still pins everything, because the submodule revision lives in the pinned commit's tree.

## The GL surface

Every entry point this game names is defined by oops-gl. A grep for `gl[A-Z]*` over `Source` finds
three names that are not calls: `glLockArraysEXT` and `glUnlockArraysEXT` are commented out at
`Source/3D/MetaObjects.c:756` and `:759`, and `glTextureName` is a local variable at
`Source/3D/OGL_Support.c:848`. Two of the three would otherwise read as a missing
`EXT_compiled_vertex_array`, which oops-gl does not have.

`gluLookAt` and `gluPerspective` are why `OOPS_FEATURES` names `glu` where Bugdom's does not.

## The loader

`Source/3D/OGL_Support.c:132` and `:135` bind `glActiveTexture` and `glClientActiveTexture` through
`SDL_GL_GetProcAddress`. Bugdom names no loader, so a missing entry point there is a link error;
here a lookup answering NULL is a null call at run time and nothing else catches it.

`make glsurface` is the check. It reads the wanted names out of the source rather than a list in the
Makefile, so a name added upstream is checked too, and it asserts in both directions: the name count
must be non-zero, and a control name that is deliberately absent must come back missing. The table is
read by expanding `OOPS_GL_PROC_LIST` with the preprocessor, so it does not matter that the list
spans two headers.

## The entry point

`FindGameData` at `Source/Boot.cpp:30` derives the data directory from `argv[0]`, accepting it only
when `Data/Skeletons/Grasshopper.bg3d` opens. `shim/bugdom2_start.cpp` probes for that file under
`/data/homebrew/<app id>` then `/app0`, sets `argv[0]` to the root that answered, exports `HOME` for
Pomme's preferences folder, and calls `oops_run_init_array()` before `main`. It is C++ because a
freestanding C++ `main` is mangled.

## Controller

`oops-deps/sdl2`'s joystick driver sends both Back and Guide from `OOPS_BUTTON_CREATE`, because
`OOPS_BUTTON_CREATE` and `OOPS_BUTTON_PS` are the same bit.
