# Q3Rally, through oops-sdk.
#
#   make census   compile every source this title needs, for the target, and report
#   make elf      not yet - see below
#
# **This does not build a payload yet, and what is missing is counted rather than guessed.**
# `make census` compiles the whole source list under the real target flags and prints what each
# one died on. docs/PORTING.md keeps the standing numbers.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# **`OOPS_SDK_DIR` early, because this file names files under it before `app.mk` is included.**
# `app.mk` defines it at its line 11, too late for an immediately-expanded (`:=`) list further
# down; the symptom is a rule wanting `/src/thread/thread.c`, rooted at nothing. The `?=` matches
# app.mk's own default, so including app.mk afterwards changes nothing.
OOPS_SDK ?= $(abspath $(OOPS_APPS_ROOT)/../oops-sdk)
OOPS_SDK_DIR := $(abspath $(OOPS_SDK))

# **`gl1`, and that was checked rather than assumed.** ioquake3 ships two renderers, and
# `renderergl1` calls no shader entry point at all - `qglCreateShader`, `qglUseProgram`,
# `qglProgramStringARB` and their kin appear zero times in it or in `renderercommon`. The
# `GL_ARB_*` strings there are extension *names* in a query table, not requirements.
# `renderergl2` is the shader renderer and is not built.
OOPS_RENDERER = gl1

# **First, because this file names files under `upstream/` while it is being read.** The fetch is
# normally done by `common/app.mk` at the bottom, which is too late for the list below.
include $(OOPS_APPS_ROOT)/common/upstream.mk

# --- the source list -------------------------------------------------------
#
# **Derived from upstream's own Makefile, not reconstructed from a glob.** ioquake3 assembles its
# client from `Q3OBJ + Q3ROBJ + JPGOBJ` across 3,420 lines of conditionals; a hand-written list
# would be a guess wearing the shape of a fact. To re-derive after a bump, ask the build system
# rather than read it:
#
#   make -n -s targets HAVE_VM_COMPILED=false \
#     USE_RENDERER_DLOPEN=0 BUILD_RENDERER_OPENGL1=1 BUILD_RENDERER_OPENGL2=0 \
#     BUILD_CLIENT=1 BUILD_SERVER=0 BUILD_GAME_SO=0 BUILD_GAME_QVM=0 BUILD_BASEGAME=0 \
#     USE_CURL=0 USE_OPENAL=0 USE_MUMBLE=0 USE_VOIP=0 USE_FREETYPE=0 \
#     USE_CODEC_VORBIS=0 USE_CODEC_OPUS=0 USE_CODEC_THEORA=0 \
#     USE_INTERNAL_ZLIB=0 USE_INTERNAL_JPEG=0 \
#   | grep -oE 'code/[A-Za-z0-9_/.-]+\.c\b' | sort -u
#
# Every switch is `ifndef`-guarded upstream, so this build's shape costs **no patch at all**:
#
#   HAVE_VM_COMPILED=false   the bytecode interpreter rather than the x86 JIT. Drops `vm_x86.c`
#                            and needs no executable memory, so `oops/jit.h` is not in play.
#                            Upstream calls this `NO_VM_COMPILED`.
#   USE_RENDERER_DLOPEN=0    link the renderer in rather than `dlopen` it; nothing here loads.
#   USE_INTERNAL_ZLIB=0      `oops-deps/zlib` instead of the bundled copy, and
#   USE_INTERNAL_JPEG=0      `oops-deps/libjpeg-turbo` instead of bundled jpeg-8c. **Not a size
#                            saving.** Two copies of `inflate` in one link resolve to whichever
#                            the linker reached first and nothing reports it - the trap
#                            `oops-stormlib.mk` records. It also removes 53 sources.
#   USE_CURL/OPENAL/MUMBLE/VOIP/FREETYPE=0, USE_CODEC_*=0
#                            single-player, no HTTP, no voice, no music yet. `openal-soft`,
#                            `libogg` and `libvorbis` are all vendored here, so audio and music
#                            are later switches rather than ports.
#
# **Two deliberate substitutions**, possible because our build names sources rather than running
# upstream's Makefile:
#
#   con_tty.c -> con_passive.c   upstream takes the tty console on every platform that is not
#                                MinGW or emscripten. There is no terminal on a console, and
#                                `con_passive.c` is upstream's own 68-line answer for that case.
#   sys_autoupdater.c            kept, and compiles to nothing - the whole file is inside
#                                `#ifdef USE_AUTOUPDATER`. Listed because upstream lists it.
Q3_UP := $(CURDIR)/upstream/engine
Q3_SRCS := $(addprefix $(Q3_UP)/,\
    code/asm/ftola.c \
    code/asm/snapvector.c \
    code/botlib/be_aas_bspq3.c \
    code/botlib/be_aas_cluster.c \
    code/botlib/be_aas_debug.c \
    code/botlib/be_aas_entity.c \
    code/botlib/be_aas_file.c \
    code/botlib/be_aas_main.c \
    code/botlib/be_aas_move.c \
    code/botlib/be_aas_optimize.c \
    code/botlib/be_aas_reach.c \
    code/botlib/be_aas_route.c \
    code/botlib/be_aas_routealt.c \
    code/botlib/be_aas_sample.c \
    code/botlib/be_ai_char.c \
    code/botlib/be_ai_chat.c \
    code/botlib/be_ai_gen.c \
    code/botlib/be_ai_goal.c \
    code/botlib/be_ai_move.c \
    code/botlib/be_ai_weap.c \
    code/botlib/be_ai_weight.c \
    code/botlib/be_ea.c \
    code/botlib/be_interface.c \
    code/botlib/l_crc.c \
    code/botlib/l_libvar.c \
    code/botlib/l_log.c \
    code/botlib/l_memory.c \
    code/botlib/l_precomp.c \
    code/botlib/l_script.c \
    code/botlib/l_struct.c \
    code/client/cl_avi.c \
    code/client/cl_cgame.c \
    code/client/cl_cin.c \
    code/client/cl_cin_ogm.c \
    code/client/cl_console.c \
    code/client/cl_curl.c \
    code/client/cl_downloads.c \
    code/client/cl_input.c \
    code/client/cl_keys.c \
    code/client/cl_main.c \
    code/client/cl_net_chan.c \
    code/client/cl_parse.c \
    code/client/cl_scrn.c \
    code/client/cl_ui.c \
    code/client/cl_update.c \
    code/client/qal.c \
    code/client/snd_adpcm.c \
    code/client/snd_altivec.c \
    code/client/snd_codec.c \
    code/client/snd_codec_ogg.c \
    code/client/snd_codec_opus.c \
    code/client/snd_codec_wav.c \
    code/client/snd_dma.c \
    code/client/snd_engine_audio.c \
    code/client/snd_engine_dsp.c \
    code/client/snd_engine_presets.c \
    code/client/snd_main.c \
    code/client/snd_mem.c \
    code/client/snd_mix.c \
    code/client/snd_openal.c \
    code/client/snd_wavelet.c \
    code/qcommon/cm_load.c \
    code/qcommon/cm_patch.c \
    code/qcommon/cm_polylib.c \
    code/qcommon/cm_test.c \
    code/qcommon/cm_trace.c \
    code/qcommon/cmd.c \
    code/qcommon/common.c \
    code/qcommon/cvar.c \
    code/qcommon/files.c \
    code/qcommon/huffman.c \
    code/qcommon/ioapi.c \
    code/qcommon/md4.c \
    code/qcommon/md5.c \
    code/qcommon/msg.c \
    code/qcommon/net_chan.c \
    code/qcommon/net_ip.c \
    code/qcommon/puff.c \
    code/qcommon/q_math.c \
    code/qcommon/q_shared.c \
    code/qcommon/unzip.c \
    code/qcommon/vm.c \
    code/qcommon/vm_interpreted.c \
    code/renderercommon/tr_font.c \
    code/renderercommon/tr_image_bmp.c \
    code/renderercommon/tr_image_jpg.c \
    code/renderercommon/tr_image_pcx.c \
    code/renderercommon/tr_image_png.c \
    code/renderercommon/tr_image_tga.c \
    code/renderercommon/tr_noise.c \
    code/renderergl1/tr_altivec.c \
    code/renderergl1/tr_animation.c \
    code/renderergl1/tr_backend.c \
    code/renderergl1/tr_bsp.c \
    code/renderergl1/tr_cmds.c \
    code/renderergl1/tr_curve.c \
    code/renderergl1/tr_flares.c \
    code/renderergl1/tr_image.c \
    code/renderergl1/tr_init.c \
    code/renderergl1/tr_light.c \
    code/renderergl1/tr_main.c \
    code/renderergl1/tr_marks.c \
    code/renderergl1/tr_mesh.c \
    code/renderergl1/tr_model.c \
    code/renderergl1/tr_model_iqm.c \
    code/renderergl1/tr_scene.c \
    code/renderergl1/tr_shade.c \
    code/renderergl1/tr_shade_calc.c \
    code/renderergl1/tr_shader.c \
    code/renderergl1/tr_shadows.c \
    code/renderergl1/tr_sky.c \
    code/renderergl1/tr_surface.c \
    code/renderergl1/tr_world.c \
    code/sdl/sdl_gamma.c \
    code/sdl/sdl_glimp.c \
    code/sdl/sdl_input.c \
    code/sdl/sdl_snd.c \
    code/server/sv_bot.c \
    code/server/sv_ccmds.c \
    code/server/sv_client.c \
    code/server/sv_game.c \
    code/server/sv_init.c \
    code/server/sv_ladder.c \
    code/server/sv_main.c \
    code/server/sv_net_chan.c \
    code/server/sv_snapshot.c \
    code/server/sv_world.c \
    code/sys/con_log.c \
    code/sys/con_passive.c \
    code/sys/sys_autoupdater.c \
    code/sys/sys_main.c \
    code/sys/sys_unix.c \
  )

# --- the dependencies we already vendor ------------------------------------
include $(OOPS_APPS_ROOT)/common/posix.mk
OOPS_ZLIB ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/zlib)
include $(OOPS_ZLIB)/oops-zlib.mk
OOPS_JPEG ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libjpeg-turbo)
include $(OOPS_JPEG)/oops-libjpeg.mk
OOPS_SDL ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/sdl2)
include $(OOPS_SDL)/oops-sdl.mk

# ioquake3 includes its own headers by bare name from several directories at once.
#
# **The `USE_*` switches are absent here, deliberately, and passing them as `=0` was a real bug.**
# Every one of them is tested with `#ifdef` - 42 sites for `USE_CURL`, 53 for `USE_VOIP` - and
# upstream's Makefile only ever adds them valueless, when the feature is *on*. So `-DUSE_CURL=0`
# defines the macro and **enables** curl. It did: 26 of this title's sources failed on
# `osreldate.h`, a header reached from `code/curl-7.54.0/include/curl/curl.h:56`, because the
# build was compiling the HTTP downloader it had been told to leave out. The switches live in the
# `make -n` invocation that derives the source list, which is where they belong; nothing needs
# them on the compile line.
#
# `STANDALONE`, not `BUILD_STANDALONE`: the latter is upstream's *Makefile* variable and the code
# tests the former (`q_shared.h`). A define nobody reads is not an error, which is why this one
# went unnoticed until the names were checked against each other.
#
# **`BOTLIB` is a per-directory flag upstream, and it is global here.** ioquake3 compiles
# `code/botlib` through its own recipe (`DO_BOT_CC`, Makefile:1504) which adds `-DBOTLIB`; without
# it `l_precomp.c`, `l_script.c` and `l_struct.c` do not see their own types. `app.mk` compiles one
# flag set for the whole payload, so it goes on globally - checked safe: nothing outside
# `code/botlib` tests the macro. The `BOTLIB` names elsewhere (`TAG_BOTLIB`, `BOTLIB_SETUP`) are
# enum constants, not the guard.
Q3_CFLAGS := -I$(Q3_UP)/code/qcommon -I$(Q3_UP)/code/client -I$(Q3_UP)/code/server \
             -I$(Q3_UP)/code/renderercommon -I$(Q3_UP)/code/renderergl1 \
             -I$(Q3_UP)/code/botlib -I$(Q3_UP)/code/sys -I$(Q3_UP)/code/sdl \
             -DNO_VM_COMPILED -DSTANDALONE -DBOTLIB \
             -DOOPS_POSIX_HOME=\"/data/q3rally\" \
             $(OOPS_POSIX_INCLUDE) $(OOPS_ZLIB_INCLUDE) $(OOPS_JPEG_INCLUDE) $(OOPS_SDL_INCLUDE)

# **Upstream's code under upstream's warnings.** `common/app.mk` compiles at `-Werror` with a set
# stricter than a 1999 codebase was written for. These are switched off rather than patched away: a
# patch against 130,000 lines of somebody else's C is a rebase burden for no behaviour change. The
# list grows only when the census names a new one.
Q3_WARNINGS := -Wno-strict-prototypes -Wno-missing-prototypes -Wno-conversion \
               -Wno-sign-conversion -Wno-shorten-64-to-32 -Wno-comment \
               -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-shadow \
               -Wno-sign-compare -Wno-char-subscripts -Wno-unused-function \
               -Wno-unused-const-variable -Wno-missing-field-initializers \
               -Wno-invalid-noreturn -Wno-unused-variable

EXTRA_TARGET_CFLAGS += $(Q3_CFLAGS) $(Q3_WARNINGS)

# --- the link --------------------------------------------------------------
#
# **Armed.** The census reached 132 of 132 on 2026-09-26 and the shim is written, so there is a
# payload to build. `common/app.mk` builds an ELF whenever `PAYLOAD_SRCS` is non-empty.
#
# `shim/q3_start.c` is the entry point and the only file here that is ours. It finds `baseq3r` by
# probing rather than deciding - the file says why, and it is because two titles in this collection
# disagree about `/app0` and both are right about themselves.
Q3_SHIM_SRCS := shim/q3_start.c

PAYLOAD_SRCS := $(Q3_SHIM_SRCS) $(Q3_SRCS) $(OOPS_POSIX_SRCS)

# **The SDK subsystems, as capabilities.** `OOPS_FEATURES` is live now - `oops-sdk.mk` ships
# `OOPS_FEATURES_AVAILABLE`, and `app.mk` turns each name into that subsystem's sources - so this
# title never names a file under `oops-sdk/src`. `net` is the one the other titles here do not
# list, and it brings `net.c`, `netctl.c` and `dns.c`; `suspend` and `draw` are what the SDL titles
# carry, and `audio` is here because ioquake3's `snd_dma.c` opens an SDL audio device on the way up
# whether or not anything is playing.
OOPS_FEATURES := display input keyboard mouse audio suspend thread draw net

# The vendored archives. `zlib` and `libjpeg-turbo` replace the copies ioquake3 bundles, which is
# why `USE_INTERNAL_ZLIB=0` and `USE_INTERNAL_JPEG=0` are in the source-list derivation above -
# two `inflate`s in one link resolve to whichever the linker reached first and nothing reports it.
EXTRA_TARGET_LDFLAGS += $(OOPS_SDL_LDFLAGS) $(OOPS_ZLIB_LDFLAGS) $(OOPS_JPEG_LDFLAGS)
PAYLOAD_EXTRA_DEPS   += $(OOPS_SDL_LIB) $(OOPS_ZLIB_LIB) $(OOPS_JPEG_LIB)

include $(OOPS_APPS_ROOT)/common/app.mk

# ---------------------------------------------------------------------------
# `make census`: which of this title's sources compile for the target, today
#
# One file at a time, `-fsyntax-only`, grouped by the **first** error per file - because one
# missing header produces a hundred of the rest, and the question is which blockers exist rather
# than how many errors they cause. Each blocker is printed **with the source that hit it**, so
# that clearing one does not start with a grep across 130,000 lines to find out where it was.
#
# **A clean census is not a working port.** `-fsyntax-only` does not link, and a payload link does
# not report an unresolved symbol, so a name nothing defines becomes a jump into nothing on the
# console rather than an error here.
#
# `common/posix/posix.c` is checked afterwards and **without `Q3_WARNINGS`**, because those switch
# off warnings a 1999 codebase was not written for and this shim was written last week. Upstream
# under upstream's warnings; our own code under ours.
# ---------------------------------------------------------------------------
.PHONY: census
census:
	@ok=0; bad=0; mkdir -p $(BUILD); : > $(BUILD)/census-errors.txt; \
	 for f in $(Q3_SRCS); do \
	    if $(TARGET_CC) $(TARGET_CFLAGS) $(EXTRA_TARGET_CFLAGS) -fsyntax-only "$$f" \
	         2>$(BUILD)/census-e.txt; then ok=$$((ok+1)); \
	    else bad=$$((bad+1)); \
	        printf '%s: %s\n' "$${f#$(Q3_UP)/}" \
	          "$$(grep -m1 'error:' $(BUILD)/census-e.txt | sed 's|.*error: ||' | cut -c1-60)" \
	          >> $(BUILD)/census-errors.txt; \
	    fi; \
	 done; \
	 printf 'q3rally census: %s compile, %s do not\n' "$$ok" "$$bad"; \
	 if [ "$$bad" -gt 0 ]; then printf '\n-- blockers --\n'; \
	    sort $(BUILD)/census-errors.txt; fi; \
	 printf '\n-- the shim, under our warnings rather than upstream ones --\n'; \
	 if $(TARGET_CC) $(TARGET_CFLAGS) $(Q3_CFLAGS) -fsyntax-only $(OOPS_POSIX_SRCS); \
	   then echo 'common/posix: ok'; else echo 'common/posix: FAILED'; exit 1; fi

# ---------------------------------------------------------------------------
# `make census-one F=<path>`: every error in one source, not just the first
#
# The census above deliberately shows one error per file. Clearing a blocker needs the opposite -
# the whole set, counted, so that "provide `fd_set`" can be told apart from "provide `fd_set` and
# the nine other names in the same header". `F` is relative to the upstream tree.
# ---------------------------------------------------------------------------
.PHONY: census-one
census-one:
	@$(TARGET_CC) $(TARGET_CFLAGS) $(EXTRA_TARGET_CFLAGS) -fsyntax-only $(Q3_UP)/$(F) 2>&1 \
	  | grep 'error:' | sed 's|.*error: ||' | sort | uniq -c | sort -rn

# ---------------------------------------------------------------------------
# `make qvm`: the game logic, which the payload does not contain and cannot build
#
# **ioquake3 is an engine, not a game.** The rules - weapons, vehicles, the HUD, the menus - live in
# three separate modules that the engine loads at run time, and none of them is in `q3rally.elf`.
# `upstream/baseq3r` ships 3,623 files of art, maps and configuration and **no compiled game logic
# at all**: no `.qvm`, no `.pk3`, no shared library. Without this step the title starts, opens the
# display, and dies in `VM_Create` - which reads like a missing file because it is one.
#
# # Why this is a host build, and why that is not a compromise
#
# A `.qvm` is bytecode for ioquake3's own virtual machine. It is **byte-identical whatever machine
# builds it** - there is no target in it to get wrong - so this compiles with the build machine's
# compiler through upstream's own toolchain (`code/tools/lcc`, a C compiler that emits Q3 bytecode,
# and `code/tools/asm`, its assembler), both of which are built here as native binaries first.
# Cross-compiling them for FreeBSD/x86-64 would produce tools that cannot run on the build machine
# to no purpose whatsoever.
#
# The engine then interprets that bytecode: `vm_interpreted.c` is in the source list and
# `NO_VM_COMPILED` is set, because the alternative - `vm_x86.c` - writes and executes machine code
# at run time and this target has no memory that is both writable and executable.
#
# `BUILD_DIR` points outside `upstream/`, because the fetched tree is fetched output and nothing
# should be written into it.
#
# Takes a few minutes: it builds four host tools and then compiles roughly 200 sources three times
# over, once per module. Separate target because nothing here needs re-running when the payload
# changes.
# ---------------------------------------------------------------------------
Q3_QVM_BUILD := $(CURDIR)/build/qvm
# The build machine's compiler, not the target's. `cc` is absent from the clang container, which is
# why this is named rather than left to upstream's default.
Q3_HOST_CC ?= clang

.PHONY: qvm
qvm:
	@$(MAKE) -C $(Q3_UP) release \
	    BUILD_DIR=$(Q3_QVM_BUILD) \
	    BUILD_CLIENT=0 BUILD_SERVER=0 BUILD_GAME_SO=0 BUILD_GAME_QVM=1 \
	    BUILD_MISSIONPACK=0 BUILD_AUTOUPDATER=0 \
	    CC=$(Q3_HOST_CC) TOOLS_CC=$(Q3_HOST_CC)
	@n=$$(find $(Q3_QVM_BUILD) -name '*.qvm' | wc -l); \
	 if [ "$$n" -ne 3 ]; then \
	    echo "q3rally: expected 3 qvm modules, found $$n" >&2; exit 1; fi
	@# **The magic number is checked, because a wrong one is the failure this step exists to
	@# prevent.** `vm.c:394` accepts `VM_MAGIC_VER2` (0x12721445) and `vm.c:415` the older
	@# `VM_MAGIC`; anything else is `Com_Error`. Reading it here means a bad q3asm is a build
	@# failure rather than a console run.
	@for f in $$(find $(Q3_QVM_BUILD) -name '*.qvm'); do \
	    m=$$(od -An -tx4 -N4 "$$f" | tr -d ' \n'); \
	    case "$$m" in 1272144[45]) ;; \
	      *) echo "q3rally: $$f has vmMagic $$m, which the engine rejects" >&2; exit 1 ;; esac; \
	  done
	@echo "q3rally: 3 qvm modules, magic checked: $$(find $(Q3_QVM_BUILD) -name '*.qvm' -printf '%f ')"

# ---------------------------------------------------------------------------
# `make pak`: one archive instead of 3,623 files
#
# **This is a deployment cost, not a tidiness preference.** `pros restore` costs per *file* rather
# than per byte - 1,775 files measured at 81 minutes - so the loose tree would be the better part of
# an afternoon every time the data moves. As one `.pk3` it is one file.
#
# A `.pk3` is a zip, which is what ioquake3 reads natively: `qcommon/unzip.c` is minizip and is in
# the payload's source list. **Paths inside it are relative to the game directory**, so this zips
# from *inside* `baseq3r` - an archive holding `baseq3r/maps/...` rather than `maps/...` is one the
# engine opens, finds nothing in, and reports as an empty pk3.
#
# The three `.qvm` modules go in under `vm/`, which is where `VM_Create` looks for them. Upstream's
# `run.sh` symlinks them to the same place.
#
# `scripts/pak.sh` does the whole of it, for the same reason `neverball` keeps `scripts/mapc.sh`
# outside its Makefile: this needs a zip that can be *appended to*, which is more shell than a make
# recipe holds legibly.
# ---------------------------------------------------------------------------
Q3_PAK_DIR  := $(CURDIR)/build/pak
Q3_PAK      := $(Q3_PAK_DIR)/baseq3r/pak0.pk3
Q3_BASEGAME := $(CURDIR)/upstream/baseq3r

.PHONY: pak
pak: qvm
	@bash scripts/pak.sh $(Q3_BASEGAME) $(Q3_QVM_BUILD) $(Q3_PAK)

# ---------------------------------------------------------------------------
# `make package`: **the one to run**, not `make title`
#
# `common/app.mk` builds a title directory out of the payload and `sce_sys/`, which is everything an
# app written here needs. A *port* also ships the program's own content, and `app.mk` has no hook for
# that - `neverball` does the same thing for the same reason.
# ---------------------------------------------------------------------------
.PHONY: package
package: title
	@test -f $(Q3_PAK) || { echo "q3rally: no $(Q3_PAK) - run 'make pak' first" >&2; exit 1; }
	@rm -rf build/title/$(TITLE_ID)/baseq3r
	@mkdir -p build/title/$(TITLE_ID)/baseq3r
	@cp $(Q3_PAK) build/title/$(TITLE_ID)/baseq3r/
	@echo "q3rally: package is build/title/$(TITLE_ID) ($$(du -sh build/title/$(TITLE_ID) | cut -f1), $$(find build/title/$(TITLE_ID) -type f | wc -l) files)"

# ---------------------------------------------------------------------------
# `make glsurface`: whether the renderer will find its GL entry points at run time
#
# **A link with nothing undefined does not answer this**, and that is the whole point of the target.
# ioquake3 fills every `qgl*` pointer from a *string* through `SDL_GL_GetProcAddress` - core names
# included - and one NULL makes `GLimp_GetProcAddresses` fail and the renderer refuse to start. The
# functions are all linked in; what decides it is whether `oops-gl`'s by-name table has them.
#
# Measured before `gl_procs_core.h` existed: 66 asked for, **0 answered**. The title would have
# booted, opened the display, printed 66 "Missing OpenGL function" lines and stopped - after a
# 200 MB deploy.
# ---------------------------------------------------------------------------
.PHONY: glsurface
glsurface:
	@bash scripts/gl-surface.sh $(Q3_UP)/code/renderercommon/qgl.h $(OOPS_SDK_DIR)/src/gl
