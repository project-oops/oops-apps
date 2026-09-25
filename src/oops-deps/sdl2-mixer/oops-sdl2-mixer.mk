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
# `MUSIC_WAV` is SDL_mixer's own decoder. **`MUSIC_OGG` plus `OGG_USE_STB`** is the bundled
# `stb_vorbis`, which decodes Ogg Vorbis with **no external libvorbis at all** - upstream's own
# option, and one fewer pinned dependency for the same files.
#
# MP3, FLAC, Opus, MIDI, MOD and Timidity are off. Extreme Tux Racer ships ten `.wav` and ten
# `.ogg` and nothing else; a decoder no title has a file for is still a parser of untrusted bytes
# and still something to carry across a bump.
#
# **`MUSIC_OGG` selects Ogg; `OGG_USE_STB` chooses which decoder answers for it.** They are not two
# decoders to pick between - this revision makes them one switch and one selector:
#
#     music_ogg.c      #if defined(MUSIC_OGG) && !defined(OGG_USE_STB)   <- libvorbis
#     music_ogg_stb.c  #if defined(MUSIC_OGG) &&  defined(OGG_USE_STB)   <- bundled stb_vorbis
#
# and **both define the same `Mix_MusicInterface_OGG`**, which `music.c` registers under
# `#ifdef MUSIC_OGG` alone. There is no `MUSIC_OGG_STB` anywhere in this tree.
#
# This file used to define `MUSIC_OGG_STB` and not `MUSIC_OGG`, on the reasoning that dropping
# `MUSIC_OGG` was what kept the libvorbis back end out. Half right, and the wrong half: the define
# it dropped is also the one that registers the interface. `music_ogg_stb.c` compiled to nothing
# behind a guard it never satisfied, no Ogg handler was registered, and `Mix_LoadMUS` answered
# NULL for every `.ogg` in the game - measured on hardware 2026-09-25, eleven of eleven, with no
# failed `open` anywhere near them because the files were read perfectly well.
#
# The lesson is the file list, not the flag: `music_ogg.c` is **not** in `OOPS_MIX_SRCS`, and that
# is what keeps libvorbis out. A define that silently compiles a translation unit to nothing looks
# exactly like a define that works.
#
# **`mp3utils.c` is built although MP3 is off**, which is not a contradiction: it is where
# `read_id3v2_from_mem` lives, and `music_wav.c` calls it to step over an ID3 tag on a WAV. The
# file is tag parsing rather than an MP3 decoder, and the decoder is still absent.
#
# **`remap_channels.c` for the same reason**: `music_ogg_stb.c` calls `remap_channels_vorbis_flt`
# to put Vorbis' channel order into SDL's, so it is part of the Ogg path rather than a codec of
# its own. The undefined-symbol check in `common/app.mk` is what named it - a payload link ignores
# unresolved symbols, so without that check this would have linked and faulted on the console.

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
    $(OOPS_MIX_UPSTREAM)/src/codecs/music_ogg_stb.c \
    $(OOPS_MIX_UPSTREAM)/src/codecs/remap_channels.c \
    $(OOPS_MIX_UPSTREAM)/src/codecs/mp3utils.c

OOPS_MIX_CFLAGS = -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib \
                  -nostdlibinc -fPIC -O2 -w -D__PROSPERO__=1 \
                  -DMUSIC_WAV -DMUSIC_OGG -DOGG_USE_STB \
                  -I$(OOPS_MIX_UPSTREAM)/src -I$(OOPS_MIX_UPSTREAM)/src/codecs \
                  $(OOPS_MIX_INCLUDE) $(OOPS_SDL_INCLUDE) \
                  $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE)

# `ar` is handed the list rather than the directory - `common/deps.mk` says what the glob cost.
$(OOPS_MIX_LIB): $(OOPS_MIX_SRCS) $(lastword $(MAKEFILE_LIST))
	@mkdir -p $(OOPS_MIX_BUILD)
	@rm -f $@
	@n=0; objs=""; for src in $(OOPS_MIX_SRCS); do \
	    n=$$((n+1)); o=$(OOPS_MIX_BUILD)/mix$$n.o; \
	    $(TARGET_CC) $(OOPS_MIX_CFLAGS) -c -o "$$o" "$$src" || exit 1; objs="$$objs $$o"; \
	done; \
	echo "sdl2-mixer: compiled $$n sources"; \
	ar_tool=$$(command -v $(AR) 2>/dev/null || command -v llvm-ar 2>/dev/null || command -v ar); \
	"$$ar_tool" rcs $@ $$objs
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
