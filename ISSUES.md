# Issues

Open defects, gaps and unmeasured facts, one line each. Delete a line when it is fixed.

- Files outside the formatter and comment pass: `common/posix/**`, `src/oops-payloads/porthole/**`, `src/oops-utilities/oopsy-daisy/**`, `src/oops-titles/bugdom/**`, `src/oops-deps/libcxx/**`; they still hold request/worklog IDs and em-dashes.
- porthole and bugdom fail `build`.
- `src/oops-payloads/porthole/docs/REFERENCE.md` links `prosperous/docs/VIDEO.md` by a wrong relative path.
- Some `@#` recipe lines in the ETR, Neverball and q3rally Makefiles print bold and narrative text.
- `home_get_category_item_info` in seashell is 183 lines.
- `SDL_prosperoevents_c.h` has no header comment.
- `common/probe_px.h`, `common/cube_frame.h`, `common/app_pad.h` and `common/app_ui.h` are candidates for oops-sdk.
- Two titles built at once corrupt each other's shared dependencies: nothing locks `src/oops-deps/*/build`, and `oops-sdl.mk`'s prefix-view rule `rm -rf`s `build/prefix/SDL2` before re-copying, so a concurrent compile reads a half-deleted header tree.
- Ship of Harkinian converts a ROM on the machine that runs it by linking ZAPD (93 sources) and OTRExporter (26); `shim/Extract.cpp` reports the missing archive instead. Spaghetti Kart is the same shape with torch.
- `BUILD_VERSION` is the clock to the minute, so a title's `.o` files carry whatever `OOPS_APP_VERSION` the link saw rather than their own; make cannot see a flag change. `common/cxx.mk` drops the macro for that reason.
