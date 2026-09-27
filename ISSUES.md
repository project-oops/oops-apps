# Issues

Open defects, gaps and unmeasured facts, one line each. Delete a line when it is fixed.

- Files outside the formatter and comment pass: `common/posix/**`, `src/oops-payloads/porthole/**`, `src/oops-utilities/oopsy-daisy/**`, `src/oops-titles/bugdom/**`, `src/oops-deps/libcxx/**`; they still hold request/worklog IDs and em-dashes.
- `src/oops-payloads/porthole/docs/REFERENCE.md` links `prosperous/docs/VIDEO.md` by a wrong relative path.
- Some `@#` recipe lines in the ETR, Neverball and q3rally Makefiles print bold and narrative text.
- `home_get_category_item_info` in seashell is 183 lines.
- `SDL_prosperoevents_c.h` has no header comment.
- `common/probe_px.h`, `common/cube_frame.h`, `common/app_pad.h` and `common/app_ui.h` are candidates for oops-sdk.
- Two titles built at once corrupt each other's shared dependencies: nothing locks `src/oops-deps/*/build`, and `oops-sdl.mk`'s prefix-view rule `rm -rf`s `build/prefix/SDL2` before re-copying, so a concurrent compile reads a half-deleted header tree.
- Ship of Harkinian converts a ROM on the machine that runs it by linking ZAPD (93 sources) and OTRExporter (26); `shim/Extract.cpp` reports the missing archive instead. Spaghetti Kart is the same shape with torch.
- Ship of Harkinian ships no `soh.o2r`, so the port has none of its own assets on the console. It needs no ROM: upstream's `GenerateSohOtr` target runs `extract_assets.py --norom`, which is the part that could run in CI. It does need a *host* ZAPD, and that is the obstacle - ZAPD links OTRExporter whole-archive, which pulls libultraship, which wants SDL2, GLEW, opus, vorbis, spdlog and tinyxml2. Measured, so the next attempt starts here: Debian (the pinned clang image) has fmt too new and `gfx_sdl2.cpp` fails on a consteval format string, and `find_package(spdlog REQUIRED)` has no vendored fallback; ubuntu:22.04 has the right fmt (8.1.1) but cmake 3.22 against a required 3.26, and pip cmake 4.x breaks `find_package` for these config files, so pin 3.31; and 22.04's `libtinyxml2-dev` ships no CMake package config at all, while upstream's vendored copy in `ZAPDTR/lib` is added after libultraship is configured.
- `BUILD_VERSION` is the clock to the minute, so a title's `.o` files carry whatever `OOPS_APP_VERSION` the link saw rather than their own; make cannot see a flag change. `common/cxx.mk` drops the macro for that reason.
