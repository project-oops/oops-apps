# SDL2_mixer build integration. Include from a title's Makefile **after** `oops-sdl.mk` and
# before `common/app.mk`:
#
#   OOPS_MIX ?= $(abspath ../../oops-deps/sdl2-mixer)
#   include $(OOPS_MIX)/oops-sdl2-mixer.mk
#
#   EXTRA_TARGET_CFLAGS  += $(OOPS_MIX_INCLUDE)
#   EXTRA_TARGET_LDFLAGS += $(OOPS_MIX_LDFLAGS)
#   PAYLOAD_EXTRA_DEPS   += $(OOPS_MIX_LIB)
#
# It needs SDL2's headers, which `oops-sdl.mk` puts on the path - this file does not include that
# one itself, because a title that wants a mixer has already decided which SDL it is using.
#
# # WAV and Vorbis, and nothing else
#
# `MUSIC_WAV` is SDL_mixer's own decoder. `MUSIC_OGG_STB` is the bundled `stb_vorbis`, which
# decodes Ogg Vorbis with **no external libvorbis at all** - upstream's own option, and one fewer
# pinned dependency for the same files.
#
# MP3, FLAC, Opus, MIDI, MOD and Timidity are off. Extreme Tux Racer ships ten `.wav` and ten
# `.ogg` and nothing else; a decoder no title has a file for is still a parser of untrusted bytes
# and still something to carry across a bump.

ifndef OOPS_MIX_DIR
OOPS_MIX_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
OOPS_MIX_UPSTREAM ?= $(OOPS_MIX_DIR)/upstream
OOPS_MIX_BUILD ?= $(OOPS_MIX_DIR)/build

OOPS_MIX_INCLUDE := -I$(OOPS_MIX_UPSTREAM)/include
OOPS_MIX_LIB := $(OOPS_MIX_BUILD)/libSDL2_mixer.a
OOPS_MIX_LDFLAGS := $(OOPS_MIX_LIB)

OOPS_MIX_SRCS := \
    $(OOPS_MIX_UPSTREAM)/src/mixer.c \
    $(OOPS_MIX_UPSTREAM)/src/music.c \
    $(OOPS_MIX_UPSTREAM)/src/utils.c \
    $(OOPS_MIX_UPSTREAM)/src/effects_internal.c \
    $(OOPS_MIX_UPSTREAM)/src/effect_position.c \
    $(OOPS_MIX_UPSTREAM)/src/effect_stereoreverse.c \
    $(OOPS_MIX_UPSTREAM)/src/codecs/load_aiff.c \
    $(OOPS_MIX_UPSTREAM)/src/codecs/load_voc.c \
    $(OOPS_MIX_UPSTREAM)/src/codecs/music_wav.c \
    $(OOPS_MIX_UPSTREAM)/src/codecs/music_ogg_stb.c

OOPS_MIX_CFLAGS = -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib \
                  -nostdlibinc -fPIC -O2 -w -D__PROSPERO__=1 \
                  -DMUSIC_WAV -DMUSIC_OGG -DMUSIC_OGG_STB \
                  -I$(OOPS_MIX_UPSTREAM)/src -I$(OOPS_MIX_UPSTREAM)/src/codecs \
                  $(OOPS_MIX_INCLUDE) $(OOPS_SDL_INCLUDE) \
                  $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE)

$(OOPS_MIX_LIB): $(OOPS_MIX_SRCS) $(lastword $(MAKEFILE_LIST))
	@mkdir -p $(OOPS_MIX_BUILD)
	@rm -f $@
	@n=0; for src in $(OOPS_MIX_SRCS); do \
	    n=$$((n+1)); \
	    $(TARGET_CC) $(OOPS_MIX_CFLAGS) -c -o $(OOPS_MIX_BUILD)/mix$$n.o "$$src" || exit 1; \
	done; \
	echo "sdl2-mixer: compiled $$n sources"
	@ar_tool=$$(command -v $(AR) 2>/dev/null || command -v llvm-ar 2>/dev/null || command -v ar); \
	 "$$ar_tool" rcs $@ $(OOPS_MIX_BUILD)/mix*.o
	@echo "sdl2-mixer: $@"

.PHONY: sdl2-mixer-clean sdl2-mixer-upstream sdl2-mixer-upstream-clean
sdl2-mixer-clean:
	@rm -rf $(OOPS_MIX_BUILD)
	@echo "sdl2-mixer: removed build/"

sdl2-mixer-upstream:
	@$(OOPS_MIX_DIR)/../../../common/upstream-fetch.sh \
	    "$$(sed -n 's/^UPSTREAM_KIND=//p' $(OOPS_MIX_DIR)/upstream.lock)" \
	    "$$(sed -n 's/^UPSTREAM_URL=//p'  $(OOPS_MIX_DIR)/upstream.lock)" \
	    "$$(sed -n 's/^UPSTREAM_REV=//p'  $(OOPS_MIX_DIR)/upstream.lock)" \
	    "$(OOPS_MIX_UPSTREAM)" "$(OOPS_MIX_DIR)/patches"

sdl2-mixer-upstream-clean:
	@rm -rf $(OOPS_MIX_UPSTREAM)
	@echo "sdl2-mixer: removed upstream/"
