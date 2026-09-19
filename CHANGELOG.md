# Changelog

oops-apps is a home for programs, not a single artifact - each app under it builds and ships
on its own. There is no repository-wide version; what a release means is decided per app, and
an app's own `README.md` is where its state is recorded.

Entries are grouped **Added / Changed / Fixed**, newest first.

Nothing has shipped yet - this is the initial state.

## [unreleased]

### Changed

- **SeaShell 60 FPS presentation & keyboard process privilege** (2026-09-18):
  - **Locked 60.0 FPS Frame Pacing**: With `oops-sdk`'s cached scratch buffer and sequential macro-tiler reducing frame work to ~16 ms, SeaShell's main loop now dynamically paces frames to 16,666 us via `oops_time_sleep_us(16666 - work_us)` for locked 60 FPS presentation without jitter.
  - **Keyboard Symbol Tagging**: Added `sceKeyboardSetProcessPrivilege` and `sceKeyboardSetProcessFocus` to `common/symbols.txt`.

- **SeaShell input overhaul & native Switcher lifecycle integration** (2026-09-18):
  - **Stick Drift Elimination**: Increased analog stick deadzone to 95 (~75% deflection, matching ItemzFlow) and granted physical D-pad presses strict priority over stick folding, preventing drifting potentiometers from hijacking navigation.
  - **Progressive Repeat Acceleration**: Implemented 3-tier repeat acceleration (18-frame initial delay, 5-frame stride, accelerating to 2-frame fast stride after 45 held frames) for responsive carousel and menu scrolling.
  - **PS Button Interception**: Integrated `OOPS_BUTTON_PS` (bit 16) with `scePadSetProcessPrivilege(1)` so SeaShell captures the PS button directly to open and close the Control Centre overlay.
  - **Native Switcher Lifecycle**: Implemented real title suspension, resume (with `SkipSystemUpdateCheck`), and termination (`oops_system_kill_app`), along with periodic background detection (`oops_system_get_running_app_title_id`) in SeaShell's main loop.
  - **Symbol Tagging**: Added `scePadSetProcessPrivilege`, `scePadGetHandle`, `sceSystemServiceGetMainAppTitleId`, `sceSystemServiceIsAppSuspended`, `sceSystemServiceKillApp`, and `sceSystemServiceGetAppIdOfBigApp` to `common/symbols.txt`.

### Fixed

- **Four probe titles no longer crash at the end of a successful run** (2026-09-17):
  `gl1-probe`, `tls-probe`, `mesa-probe` and `dri-probe` now end by calling oops-sdk's
  `oops_system_park_until_closed()` instead of returning from their entry point.

  Returning faulted with `SIGSEGV` at `rip: 0x0` every time, because the dynamic linker gives a
  title's entry point no caller frame to return into - and no userland call ends a `big-app`
  process either, since lifecycle belongs to `SceShellCore` (obSCEne `REQ-20260917T1450Z-2e71`,
  measured on retail firmware 12.40). `gl1-probe`'s own comment had described both failures
  accurately and said returning was what it would do "until the answer arrives"; this is the
  answer arriving.

  The crash was always *after* every result was printed, so it never cost a measurement. What it
  cost was a clean log tail and a coredump per run, and it made a good run look like a bad one.

  **`gl1-probe` stops returning a pass/fail exit code, and nothing is lost.** There is no
  launcher left to read one - the process cannot exit to hand it over - and the verdict was never
  only in the code: `report_total` prints `gl1-probe: NN/NN passed on hardware`, which the
  resolution names as the completion sentinel a harness watches for. The host build still
  returns the code, because a host process has a real lifecycle.

  Not applied to `gl1-cube`, `gallery`, `net-tool`, `pad-viz`, `porthole` or `seashell`, and that
  is a decision rather than an oversight. For a harness-driven probe the old ending is a crash
  report attached to a good result and parking is strictly better. For a title a person quits,
  the crash at least ends the process and returns the console to the dashboard, where parking
  would leave it frozen on screen. The right ending for those is
  `sceSystemServiceNavigateToGoHome()` - the alternative the same resolution names, already bound
  weakly in oops-sdk - which is a different contract and a judgement about each title's users.

### Changed

- **`common/app.mk` filters flags out of the Mesa dependency list** (2026-09-17):
  `PAYLOAD_EXTRA_DEPS` now takes `$(filter-out -%,$(OOPS_MESA_LIBS))`. That list is mostly
  archive paths but also carries the `--whole-archive` pair bracketing Mesa's public GL entry
  points, and a flag named as a prerequisite is a target make has no rule for. Only the files are
  dependencies; the flags were already in `LDFLAGS`.

### Added

- **Home-screen presentation assets and subtitles for flat title packaging** (2026-09-17):
  `common/app.mk` now auto-detects and forwards title presentation metadata to `selfish`:
  * Background wallpaper (`pic0.png`): automatically picked up from `src/<category>/<app>/pic0.png`
    or configured via `TITLE_PIC0` in `app.env`.
  * Secondary logo graphic (`logo.png`): automatically picked up from `src/<category>/<app>/logo.png`
    or configured via `TITLE_LOGO` in `app.env`.
  * Title subtitle: configured via `APP_SUBTITLE` in `app.env` or `TITLE_SUBTITLE` in the Makefile,
    written directly into `param.json`'s `titleSubName`.
  When not provided, `selfish` automatically synthesizes conforming fallback assets (ambient 1080p
  background, translucent glass title badge, and default subtitle), eliminating the OS grey void
  (`bg_hub_default.dds`) across all packaged flat homebrew titles.
- **Six more gl1-probe checks, 27 to 33** (2026-09-17), each covering something oops-gl
  implements on both paths and nothing had ever drawn with:
  * `cull-face` - `GL_CULL_FACE` appeared once in the whole file before this, in the reset
    helper, being *disabled*, and `glCullFace`/`glFrontFace` were never called at all, although
    gl1-cube leans on both. Deliberately does **not** assert which winding is front: window space
    here is Y-flipped, so writing that down would make it a test of my arithmetic. It asserts the
    relationship the specification fixes - exactly one of an opposite-wound pair survives,
    `glFrontFace` swaps which, `GL_FRONT_AND_BACK` removes both - against a culling-off control.
  * `viewport` - `glViewport` had never been called, so every check drew into the whole window
    and a viewport applied as a scale but not an offset looked identical. Samples two pixels
    either side of the left boundary.
  * `depth-mask` - `glDepthMask` had never been called. Asserts both outcomes rather than only
    that they differ, so an inverted mask fails too.
  * `texture-wrap` - the wrap mode was set and read back through `glGetTexParameteriv`, which
    proves the field round-trips and nothing about the sampler. Runs coordinates out to `s = 2`
    and requires the out-of-range sample to agree with the *first* column under `GL_REPEAT` and
    the *last* under `GL_CLAMP_TO_EDGE`.
  * `tex-sub-image` - `glTexSubImage2D` has been on the roadmap as done since it landed and
    nothing had drawn with it. A 2x2 patch into one quadrant of a 4x4: exactly one of four
    quadrants may change.
  * `two-lights` - `GL_LIGHT1` appeared only in a `glLightfv`/`glGetLightfv` round-trip, so
    whether a second light contributes light was untested while `GL_MAX_LIGHTS` reports eight.
    Both lights dim and on separate channels, so saturation cannot hide the answer.

  Each was mutation-checked against the rasteriser: culling ignored, the viewport offset dropped,
  the depth mask forced true, the wrap mode forced to `GL_REPEAT`, `glTexSubImage2D` made a
  no-op, and the light loop cut to one - each turning its own check, and only its own, red.
  (`glTexSubImage2D` also takes `copy-tex` with it, since `glCopyTexSubImage2D` goes through it.)
- **gl1-cube's host self-test runs the payload's scene, with the payload's lighting**
  (2026-09-17): it used to configure depth, culling and texturing and then stop, so the app's
  headline state - `GL_LIGHTING` with `GL_COLOR_MATERIAL`, a specular term and a normal array -
  was exercised nowhere on the host and a rasteriser that ignored the light entirely would have
  passed. It now sets up the light exactly as `gl1_cube_main.c` does and requires that disabling
  it changes the frame (it moves 86,833 of the 109,031 drawn pixels), builds and rasterises the
  torus and sphere the payload's R2 button switches to - neither of which had ever been generated
  off-target - and requires that redrawing the same scene produces an identical frame, which is
  the property the oracle record's hash depends on. Each of the three was mutation-checked.
- **gl1-cube control file `gputile`** (2026-09-17): move display tiling from the CPU to the GPU's
  compute tiler for this run, through oops-sdk's new `oops_display_try_gpu_tiler()`. The HUD's
  new `Tile: CPU|GPU` field says which path a run is on. It is a control file rather than the
  default because what the CPU tiler costs is unmeasured: one run with the file and one without
  give `t-swap-us` for each, and the difference is the number. Until it exists the default stays
  the path the oracle record was measured on.
- **gl-cube control files `preamble-a` and `preamble-b`** (2026-09-14): open every frame's
  command stream with the preamble Mesa's AMD driver would emit for this hardware, generated by
  the sibling oops-mesa checkout and picked up when it is present. The instrument for oops-mesa's
  route measurement (oops-mesa#D003, worklog 002), which passed. gl-cube's source list now names
  the SDK's `math.c` and `fs.c`, which SELFish's import check had started refusing as unresolved.
- **A repository for the homebrew this collection builds on its own SDK.** Something a person
  runs on the hardware for its own sake - a tool, a demo, a service - built on
  [oops-sdk](https://github.com/project-oops/oops-sdk). The admission rule is deliberately
  narrow: a probe that exists to *measure* the platform belongs in obSCEne, an app that *does*
  something belongs here. (D001)
- **Porthole, moving in from obSCEne.** The target half of the capture-and-input path whose
  host half lives in Prosperous - it was a payload inside a probe and is now an app in its own
  right, which is what prompted the repository. The freestanding base layer it needs came in
  with it on loan, and belongs in oops-sdk rather than here. (D002)

### Changed

- **Porthole's target sockets are the platform's POSIX ones, not libSceNet.** obSCEne dumped the
  export table an elfldr payload is handed on 2026-09-08, and none of libSceNet is in it - not
  `sceNetSocket`, not one of the nine the payload needed. The POSIX layer is: `bind`, `listen`,
  `accept`, `recv`, `_sendto`, `_setsockopt`, `close`, `fcntl` and `__error`, at real addresses.
  So the payload now reaches sockets the way its own host build always did, both halves speak one
  shape, and the two spellings the platform uses are tried per call rather than one firmware's
  naming being baked in. `socket` itself was in none of seventeen spellings, so it is resolved by
  name where offered and otherwise asked of the kernel, with the klog naming which route worked.
  The socket itself comes from `__sys_socketex`, which takes a name the way the vendor's own
  creator does and was found under a spelling none of seventeen guesses reached; the raw system
  call is kept behind it, both routes having returned a working descriptor on hardware.
- **Porthole sets a socket's mode with the socket option, because `fcntl` cannot do it here.**
  Measured on this console: `fcntl(F_SETFL, O_NONBLOCK)` answers -1 while
  `setsockopt(SOL_SOCKET, SO_NBIO)` answers 0. Reading flags back with `F_GETFL` does work, so
  this is a platform that will tell you a socket's mode but not let you set it that way. `fcntl`
  is still tried second, for a firmware where the option is the half that is missing.
- **A full send buffer is waited out rather than treated as a dead connection.** An accepted
  socket inherits the listener's non-blocking mode here, confirmed by reading the flags back off
  one, so the video connection is put back to blocking; should that ever fail, a full buffer
  answers try-again and closing on it would drop a working connection whenever the network fell
  behind. Giving up mid-unit is not an option either, since half an access unit on the wire is
  the corruption the send loop exists to prevent.
  The virtual-port field that made two sockaddr shapes disagree is no part of the POSIX struct,
  so that question does not arise on this path.
- **Porthole's encoder session is off unless a build asks for it.** The four struct-taking
  encoder calls pass parameter layouts not yet confirmed against the platform, which obSCEne's
  D300 had reserved for M2, yet every start made two of them and every frame the other two. They
  are now compiled in only with `PORTHOLE_ENCODER_SESSION`, a default build serves the template
  stream, and the selftest asserts the default. (D003)

### Added

- **gl1-probe's first hardware results: 26 of 34** (2026-09-17). The suite ran end to end on a
  retail console - 55 submissions, every fence returned `0xbeefcafe`, `canary-vs 0xbeef0001` and
  `canary-ps 0xbeef0002` on every drawing check - and the eight failures are three causes, not
  eight:

  | Failing | Cause |
  |---|---|
  | `scissor` | `glScissor` never reaches the GPU; the scissor registers are patched from the render target's size, the same way `glViewport` was until this day |
  | `blend`, `blend-equation` | `CB_BLEND0_CONTROL` is the constant `0x00002504` whenever blending is enabled, so `glBlendFunc` and `glBlendEquation` are software-only |
  | `texture-2d`, `tex-env-modes`, `copy-tex`, `tex-sub-image`, `alpha-test` | not yet diagnosed. `texture-wrap` and `tex-delete-in-frame` pass, and both sample a texture, so sampling works at least for one row of texels |

  An earlier run of the same suite scored 8 of 34, and the difference was a single bug: the
  hardware path ignored `glViewport`, so every check that compared a pixel to an expected colour
  was reading the wrong part of the frame. The two checks that compare one frame against another
  passed in both runs, which is what named the cause.
- **gl1-probe leaves through the kernel instead of returning** (2026-09-17). Returning from the
  entry point faulted on every hardware run - `rip: 0x0`, "user read instruction, page not
  present", with the return value still in `rax` and the pass count in `rdx`. It had finished the
  suite and printed every result, then returned to an address that is not there: the loader gives
  this payload no return path, and the container's `eh_frame_hdr` is reported corrupt at load, so
  there is nothing to unwind into either. It cost no results, but it produced a crash dialog, a
  coredump and a GPU query every run, and a probe that ends in a crash report is hard to read as
  a probe that passed.

### Fixed

- **gl1-probe opened a display the hardware will not scan out** (2026-09-17), found on its first
  launch. It asked for **128x96** - the size its checks work in - which is perfect on the host
  and impossible on a console: `libSceVideoOut` refuses to register a buffer of that size.
  oops-sdk already knew the shape of this and says so in `agc_display.c`, where a requested
  1280x720 is promoted to 1080p because "720p buffer registration is refused unless the console
  is configured for 720p scanout" and "universal 1080p is supported across all output modes".
  Nothing promotes 128x96. Every other app here that runs on hardware opens 1920x1080.

  The probe now opens the display at 1920x1080 and draws into a 128x96 viewport **at GL's
  origin**. Keeping it at the origin is what made the change small: the rasteriser flips
  `glScissor` against the full framebuffer height, so a scissor box at the origin lands in the
  probe's own region exactly as it did when the framebuffer *was* the probe's size, and all 33
  checks keep their coordinates. Only `px()` learns where the region starts.
- **gl1-probe never asked whether its display opened** (2026-09-17). `oops/display.h` says a
  backend that could not open is returned rather than hidden, and to check
  `oops_display_is_ready()`; gl1-cube always did and the probe never did. So the refused open
  above produced a NULL framebuffer that looked like a working one, and the first check to read
  it took the process down with `SIGSEGV` at address 0. Checked now, along with the framebuffer
  pointer and the display's reported size.
- **The probe's whole-frame comparison helpers dereferenced a NULL frame** (2026-09-17).
  `px()` guarded itself; `frame_snapshot()` and `frame_matches()`, added the same day, did not -
  so they were the instruction that actually faulted. Both check now, and both walk the probe's
  own rectangle rather than the whole display.
- **gl1-probe staged onto the console perfectly and was never installed, because its title id
  was not a title id** (2026-09-17). `GL1P00001` looks like one and is not: the vendor format is
  four capital *letters* then five digits, and `GL1P` has a `1` among the letters. Every file
  landed, the directory was complete, `shadowmountplus` even nullfs-mounted it - and the console
  refused to index it, saying so only in its own log:

  ```
  20 Invalid TitleId : [GL1P00001]
  AppPromote Error [GL1P00001] ret = [0x80bd000a]
  AppInstallTitleDirMain GL1P00001 0x80bd000a
  ```

  while `GLCB00001`, `MESA00001`, `TLSP00001` and `PLDM00001` - four letters each - indexed
  normally beside it. Renamed `GLPB00001`, redeployed with nothing else changed, and it appeared
  in `pros titles` within ten seconds. `gl2-cube` carried the same defect in `GL2C00001` and is
  now `GLTC00001`; it had never been packaged, so it was caught before it cost anything.

  Both ids were invented the same day to dodge a `PROG00001` collision between `gallery`,
  `gl1-probe` and `gl2-cube`. The collision was real and the fix for it was right; the ids chosen
  were not, and nothing checked them.
- **`app.mk` warned about a malformed title id where it should have refused** (2026-09-17). The
  shape check existed and was deliberately a warning, on the stated grounds that "whether the
  loader minds is not known - neither has been deployed". That was the right call to make on no
  evidence, and it is why gl1-probe was built and deployed with an id that could not install.
  The evidence now exists, so it is an error, and the comment records the log lines it was
  measured from rather than the reasoning it replaced. `EXPLICIT_TITLE_ID=1` is the only branch
  that needs it: the synthesis path cannot produce a malformed id.
- **gl1-probe could not have worked on a console, which is the only place it is for**
  (2026-09-17). Every check sampled the render target directly and nothing in the file called
  `glFinish`, `glFlush` or `glSwapBuffers`, or read through `glGetFrameReadback()`. On hardware
  the draw path builds a command stream and returns - oops-sdk's `gl_draw.c` says so in as many
  words, "the software rasterizer stands in for it there, and only there" - so nothing has
  touched the target until the stream is submitted and its end-of-pipe fence returns. `glClear`
  takes the same split, so the checks would not merely have missed what they drew, they would
  have compared against a background that was never painted, and `clear-and-rect` would have
  failed first. A deployment would have reported something close to 0/27 and read as "oops-gl's
  hardware path is broken end to end" when the probe had never asked the GPU to do anything.

  Sampling now goes through one `frame()` helper that finishes the frame and reads the command
  processor's cached copy, so no check has to remember to; a `glFinish()` with nothing pending is
  a no-op, so the cost is one submission per draw-then-sample sequence. The six whole-framebuffer
  comparisons that indexed the target directly go through the same helper. The host result is
  unchanged at every step, by construction - which is why nothing had caught it.
- **gl1-cube's self-test was rasterising a cube the console never drew** (2026-09-17). The
  vertex, colour, texture-coordinate and normal arrays and the procedural texture were written
  out twice, once in the payload and once in the test, and a comparison of the two found them
  drifted: the top face's sixth vertex is `1.00, 1.00, 0.15` in the payload and was
  `0.15, 1.00, 0.15` in the test. One value in 288, silent, and it meant the host gate for the
  pinned hardware oracle had not been testing the oracle's scene. Both now include
  `gl1_cube_scene.h` and there is one copy. The payload's values are the ones kept, because they
  are what the record measured.
- **gl1-cube's "Mod Pix" was the sum of two different measurements of the same frame**
  (2026-09-17). Pixels outside the clear colour were counted in full from the readback and then
  counted again, one word in 64, from the render target, into the same variable - so the number
  on screen was neither figure. The second scan is gone; its 32,400 reads of uncached
  write-combined memory were also the most expensive thing in the block that produced the wrong
  half of it. The oracle record is unaffected and its published counts stand: `oracle-mod-pixels`
  is logged above that point, from the readback alone.
- **gl1-cube hashed all 2,073,600 words of every frame, in every run** (2026-09-17), having first
  invalidated the 129,600 cache lines holding them - the price of the oracle record, paid by runs
  that record nothing. The full pass now runs only under `pause` or `dump`, the runs whose
  numbers are logged and compared; every other run samples one word in 64 and invalidates one
  cache line in four. The HUD labels the sampled figures `Hash(1:64)` and `Mod Pix(1:64)`, because
  a sampled hash and a full-frame hash are not comparable and a reader who took one for the other
  would conclude the frame had changed when it had not.
- **gl1-cube's banner claimed OpenGL 2.0** (2026-09-17). It is the fixed-function demo; the
  string dated from before the app was split into gl1-cube and gl2-cube, and every screenshot of
  it advertised a shader pipeline nothing in it uses. It says `OOPS-GL 1.x` now. In the same pass,
  a geometry mode whose mesh failed to allocate no longer reports the cube's name and twelve
  triangles over an empty screen.
- **Porthole serves without the encoder, which is the only way the gate works.** `porthole_run`
  refused to start unless the encoder opened, and `porthole_capture_encode` refused unless it
  had, so a gated payload would have exited without opening a socket - the opposite of what the
  gate was for. obSCEne measured on 2026-09-08 that no `sceVencCore` entry point resolves on any
  delivery route, and that none of the twenty-four is in the export table an elfldr payload is
  handed, so this was not hypothetical. The encoder is now attempted and logged, never required; gated off, the
  template stream serves. A gate-on build still refuses, having been asked for video it cannot
  produce. (D003)
- **The startup log no longer announces a pad subsystem it did not reach.** Opening the pad
  always returned success, and the entry point took that as licence to print "self-resolved
  successfully" above whatever addresses it had - which on this hardware, where none of the four
  controller symbols is in a payload's export table, meant announcing success above four zeroes.
  Opening now reports `PORTHOLE_NO_PAD` when the add and insert calls are missing, since without
  those there is no input path at all, and the log prints the addresses either way with a
  headline saying which case it is. Records are still read, checked and sequence-tracked; they
  are just not injected, and the log now says so.
- **A controller record of an unknown version is refused rather than interpreted.** The host half
  has refused a version mismatch from the start and the payload accepted anything, so the field
  meant nothing on the receiving end and a later version's gyro bytes would have been read as
  reserved zeroes.
- **A video frame is sent until all of it has gone.** A short write dropped the tail of an
  access unit, which the host's reader resynchronises past - so the loss would have surfaced as
  a stuttering picture rather than as an error anywhere. The template units are too small for it
  to bite today; real frames are not.
- **One access unit gets a second of the configured bitrate.** The frame buffer was four
  kilobytes on the stack, which no 1080p keyframe fits, so with the encoder gate on every real
  access unit would have been refused and the test pattern served instead - a stream that frames
  and keyframes perfectly while showing colour bars, which is the one fault the host cannot tell
  from the outside. It is now static, because a payload's stack will not hold a megabyte and
  there is no allocator, and sized from `PORTHOLE_DEFAULT_BITRATE` so the two cannot drift
  apart. A compile-time floor refuses a configuration too small for a keyframe, and an access
  unit that still does not fit names its size in the log rather than quietly becoming a test
  pattern.

- **Porthole's serving loop no longer waits on its input socket.** A receive that blocked held
  the video frame with it, so the stream stalled whenever a pad was at rest and the host went
  quiet; a blocking accept meant no video at all until an input client also connected. The
  listeners are now non-blocking, the input receive takes only what has arrived, and the video
  connection is set to block explicitly.
- **A host that reconnects is a new sender.** Each slot's last sequence is forgotten when a new
  input connection is accepted, so a restarted host's records are not dropped as stale against
  the previous sender's high mark. A superseded record now reports `PORTHOLE_STALE` rather
  than looking applied, which is what lets the selftest tell the two apart.
