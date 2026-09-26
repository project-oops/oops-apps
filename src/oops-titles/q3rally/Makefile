# Q3Rally, through oops-sdk.
#
#   make elf        the plain-ELF payload a homebrew loader runs
#   make title      complete title package (.zip)
#   make qvm        the game-logic bytecode modules, built on the host
#   make pak        the game data and modules as one .pk3
#   make package    the title package with the .pk3, which is what runs
#   make census     compile every source for the target and report the first error of each
#   make glsurface  check that oops-gl answers every GL name the renderer looks up

OOPS_APPS_ROOT ?= $(abspath ../../..)

# Set early: this file names files under `OOPS_SDK_DIR` before `app.mk` defines it. The `?=`
# matches app.mk's own default.
OOPS_SDK ?= $(abspath $(OOPS_APPS_ROOT)/../oops-sdk)
OOPS_SDK_DIR := $(abspath $(OOPS_SDK))

# `renderergl1` and `renderercommon` call no shader entry point; the `GL_ARB_*` strings there
# are extension names in a query table. `renderergl2` is the shader renderer and is not built.
OOPS_RENDERER = gl1

# First, because the list below names files under `upstream/`, and `common/app.mk` (which
# normally fetches it) is included at the bottom.
include $(OOPS_APPS_ROOT)/common/upstream.mk

# --- the source list -------------------------------------------------------
#
# Derived from upstream's Makefile, which assembles the client from `Q3OBJ + Q3ROBJ + JPGOBJ`.
# To re-derive after a bump:
#
#   make -n -s targets HAVE_VM_COMPILED=false \
#     USE_RENDERER_DLOPEN=0 BUILD_RENDERER_OPENGL1=1 BUILD_RENDERER_OPENGL2=0 \
#     BUILD_CLIENT=1 BUILD_SERVER=0 BUILD_GAME_SO=0 BUILD_GAME_QVM=0 BUILD_BASEGAME=0 \
#     USE_CURL=0 USE_OPENAL=0 USE_MUMBLE=0 USE_VOIP=0 USE_FREETYPE=0 \
#     USE_CODEC_VORBIS=0 USE_CODEC_OPUS=0 USE_CODEC_THEORA=0 \
#     USE_INTERNAL_ZLIB=0 USE_INTERNAL_JPEG=0 \
#   | grep -oE 'code/[A-Za-z0-9_/.-]+\.c\b' | sort -u
#
# Every switch is `ifndef`-guarded upstream, so none needs a patch.
# HAVE_VM_COMPILED=false selects the bytecode interpreter; `vm_x86.c` needs executable memory.
# USE_RENDERER_DLOPEN=0 links the renderer in.
# USE_INTERNAL_ZLIB=0 and USE_INTERNAL_JPEG=0 use `oops-deps/zlib` and
# `oops-deps/libjpeg-turbo`: two copies of `inflate` in one link resolve silently to one.
# USE_CURL, OPENAL, MUMBLE, VOIP, FREETYPE and USE_CODEC_* are off: single-player, no HTTP,
# no voice, no music.
#
# `con_passive.c` replaces `con_tty.c`: there is no terminal, and it is upstream's own
# console for that case. `sys_autoupdater.c` compiles to nothing (`#ifdef USE_AUTOUPDATER`)
# and is listed because upstream lists it.
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
# No `USE_*` defines: the code tests each with `#ifdef`, so `-DUSE_CURL=0` would enable curl.
# The switches belong only to the `make -n` derivation above.
# `STANDALONE`, not `BUILD_STANDALONE`: the code tests the former (`q_shared.h`); the latter
# is upstream's Makefile variable.
# `BOTLIB` is per-directory upstream (`DO_BOT_CC`, Makefile:1504) and global here, since
# `app.mk` compiles one flag set. Nothing outside `code/botlib` tests it.
Q3_CFLAGS := -I$(Q3_UP)/code/qcommon -I$(Q3_UP)/code/client -I$(Q3_UP)/code/server \
             -I$(Q3_UP)/code/renderercommon -I$(Q3_UP)/code/renderergl1 \
             -I$(Q3_UP)/code/botlib -I$(Q3_UP)/code/sys -I$(Q3_UP)/code/sdl \
             -DNO_VM_COMPILED -DSTANDALONE -DBOTLIB \
             -DOOPS_POSIX_HOME=\"/data/q3rally\" \
             $(OOPS_POSIX_INCLUDE) $(OOPS_ZLIB_INCLUDE) $(OOPS_JPEG_INCLUDE) $(OOPS_SDL_INCLUDE)

# Upstream's code under upstream's warnings. `common/app.mk` compiles at `-Werror` with a set
# upstream does not meet; these are switched off rather than patched away.
Q3_WARNINGS := -Wno-strict-prototypes -Wno-missing-prototypes -Wno-conversion \
               -Wno-sign-conversion -Wno-shorten-64-to-32 -Wno-comment \
               -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-shadow \
               -Wno-sign-compare -Wno-char-subscripts -Wno-unused-function \
               -Wno-unused-const-variable -Wno-missing-field-initializers \
               -Wno-invalid-noreturn -Wno-unused-variable

EXTRA_TARGET_CFLAGS += $(Q3_CFLAGS) $(Q3_WARNINGS)

# --- the link --------------------------------------------------------------
#
# `shim/q3_start.c` is the entry point and the only source here that is ours.
Q3_SHIM_SRCS := shim/q3_start.c

PAYLOAD_SRCS := $(Q3_SHIM_SRCS) $(Q3_SRCS) $(OOPS_POSIX_SRCS)

# SDK subsystems by feature; `app.mk` supplies their sources. `net` brings the network stack,
# and `audio` is needed because `snd_dma.c` opens an SDL audio device at start-up.
OOPS_FEATURES := display input keyboard mouse audio suspend thread draw net

# The vendored archives, replacing the copies ioquake3 bundles (see USE_INTERNAL_* above).
EXTRA_TARGET_LDFLAGS += $(OOPS_SDL_LDFLAGS) $(OOPS_ZLIB_LDFLAGS) $(OOPS_JPEG_LDFLAGS)
PAYLOAD_EXTRA_DEPS   += $(OOPS_SDL_LIB) $(OOPS_ZLIB_LIB) $(OOPS_JPEG_LIB)

include $(OOPS_APPS_ROOT)/common/app.mk

# ---------------------------------------------------------------------------
# `make census`: which of this title's sources compile for the target
#
# One file at a time, `-fsyntax-only`, reporting the first error of each file with its source.
# It does not link, and a payload link does not report unresolved symbols.
# `common/posix` is checked afterwards without `Q3_WARNINGS`: our code under our warnings.
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
# Every distinct error in one file, counted. `F` is relative to the upstream tree.
# ---------------------------------------------------------------------------
.PHONY: census-one
census-one:
	@$(TARGET_CC) $(TARGET_CFLAGS) $(EXTRA_TARGET_CFLAGS) -fsyntax-only $(Q3_UP)/$(F) 2>&1 \
	  | grep 'error:' | sed 's|.*error: ||' | sort | uniq -c | sort -rn

# ---------------------------------------------------------------------------
# `make qvm`: the game logic, which the payload does not contain
#
# The game rules live in three modules the engine loads at run time (`VM_Create`), and
# `upstream/baseq3r` ships no compiled ones. A `.qvm` is bytecode for ioquake3's virtual
# machine, identical whatever machine builds it, so upstream's own toolchain (`code/tools/lcc`
# and `code/tools/asm`) builds natively on the host. The engine interprets it
# (`NO_VM_COMPILED`). `BUILD_DIR` keeps the output out of the fetched `upstream/` tree.
# ---------------------------------------------------------------------------
Q3_QVM_BUILD := $(CURDIR)/build/qvm
# The host compiler, named because the clang container has no `cc`.
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
# `make pak`: the game data as one archive
#
# `pros restore` costs per file, so the data deploys as one `.pk3` (a zip, read by
# `qcommon/unzip.c`). Paths inside are relative to the game directory, and the `.qvm` modules
# go under `vm/`, where `VM_Create` looks. `scripts/pak.sh` builds it.
# ---------------------------------------------------------------------------
Q3_PAK_DIR  := $(CURDIR)/build/pak
Q3_PAK      := $(Q3_PAK_DIR)/baseq3r/pak0.pk3
Q3_BASEGAME := $(CURDIR)/upstream/baseq3r

.PHONY: pak
pak: qvm
	@bash scripts/pak.sh $(Q3_BASEGAME) $(Q3_QVM_BUILD) $(Q3_PAK)

# ---------------------------------------------------------------------------
# `make package`: the title package from `common/app.mk` plus the game's `.pk3`
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
# ioquake3 fills every `qgl*` pointer by name through `SDL_GL_GetProcAddress`, core names
# included, and one NULL makes `GLimp_GetProcAddresses` fail. A clean link does not show this;
# the answer is whether oops-gl's by-name table has each name.
# ---------------------------------------------------------------------------
.PHONY: glsurface
glsurface:
	@bash scripts/gl-surface.sh $(Q3_UP)/code/renderercommon/qgl.h $(OOPS_SDK_DIR)/src/gl
