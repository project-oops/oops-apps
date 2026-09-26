# Issues

Open defects, gaps and unmeasured facts, one line each. Delete a line when it is fixed.

- Files outside the formatter and comment pass: `common/posix/**`, `src/oops-payloads/porthole/**`, `src/oops-utilities/oopsy-daisy/**`, `src/oops-titles/bugdom/**`, `src/oops-deps/libcxx/**`; they still hold request/worklog IDs and em-dashes.
- porthole and bugdom fail `build`.
- `src/oops-payloads/porthole/docs/REFERENCE.md` links `prosperous/docs/VIDEO.md` by a wrong relative path.
- Some `@#` recipe lines in the ETR, Neverball and q3rally Makefiles print bold and narrative text.
- `home_get_category_item_info` in seashell is 183 lines.
- `SDL_prosperoevents_c.h` has no header comment.
- `common/probe_px.h`, `common/cube_frame.h`, `common/app_pad.h` and `common/app_ui.h` are candidates for oops-sdk.
