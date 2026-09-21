# sdl12-compat build integration: SDL 1.2's API over SDL2, for a title that was written against
# SDL 1.2 and is not going to be rewritten. Include from a title's Makefile, before
# `common/app.mk`:
#
#   OOPS_SDL12 ?= $(abspath ../../oops-deps/sdl12-compat)
#   include $(OOPS_SDL12)/oops-sdl12.mk
#
#   EXTRA_TARGET_CFLAGS  += $(OOPS_SDL12_INCLUDE)
#   EXTRA_TARGET_LDFLAGS += $(OOPS_SDL12_LDFLAGS)
#   PAYLOAD_EXTRA_DEPS   += $(OOPS_SDL12_LIB) $(OOPS_SDL12_SDL2_LIB)
#
# **Do not include `oops-sdl.mk` as well.** This file pulls in the SDL2 it needs and hands back a
# *renamed* copy of it; a title that linked both would get two SDL2s and the collision described
# below. A title uses SDL 1.2 or SDL 2, not both.
#
# # Why SDL2 has to be renamed, and why the new names are upstream's own
#
# sdl12-compat is built to be a drop-in *shared library*: it defines SDL 1.2's `SDL_Init` and
# reaches SDL2's `SDL_Init` through `dlopen`, so the two never meet in one symbol table. Linked
# statically into a payload they meet immediately - **233 of the names sdl12-compat defines are
# also SDL2's**, starting with `SDL_Init` itself.
#
# It cannot be solved on the sdl12-compat side. Its own `SDL_Init` must keep that name, because
# that is the name the title calls. So SDL2's copy is what moves.
#
# The new name was already chosen, by upstream. `src/SDL20_include_wrapper.h` includes SDL2's
# headers with every declaration renamed to `IGNORE_THIS_VERSION_OF_SDL_*` - so the C code can
# see both APIs at once - and then `#undef`s the renames so the SDL 1.2 names are free. Renaming
# SDL2's *linker* symbols the same way makes the declarations sdl12-compat already has resolve to
# the definitions it already wants, with no invention and no third naming scheme to remember.
#
# One `objcopy --redefine-syms` pass over the archive does it, from a map `nm` generates, so the
# list is read from the build rather than maintained by hand. 1148 symbols move.

ifndef OOPS_SDL12_DIR
OOPS_SDL12_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
OOPS_SDL12_UPSTREAM ?= $(OOPS_SDL12_DIR)/upstream
OOPS_SDL12_BUILD ?= $(OOPS_SDL12_DIR)/build

# The SDL2 underneath. Included for its file list, its config and its backends - everything
# except its archive, which is rebuilt here under the renamed symbols.
OOPS_SDL ?= $(abspath $(OOPS_SDL12_DIR)/../sdl2)
include $(OOPS_SDL)/oops-sdl.mk

OOPS_SDL12_INCLUDE := -I$(OOPS_SDL12_UPSTREAM)/include

OOPS_SDL12_LIB := $(OOPS_SDL12_BUILD)/libSDL12compat.a
OOPS_SDL12_SDL2_LIB := $(OOPS_SDL12_BUILD)/libSDL2-renamed.a

# Upstream's own warning flags, for the same reason `oops-sdl.mk` gives: this repository's
# `-Werror -Wsign-conversion` is ours and not theirs. `SDL12_compat.c` also needs SDL2's headers,
# which it includes through its wrapper.
OOPS_SDL12_CFLAGS = $(filter-out $(OOPS_SDL_WARN_DROP),$(TARGET_CFLAGS)) -w \
                    $(OOPS_SDL_INCLUDE) $(OOPS_SDL12_INCLUDE)

# **Not `--whole-archive`.** `oops-sdl.mk` needs it because SDL2's driver bootstraps are only
# reachable through tables nothing references by name. Here the title references SDL 1.2 names
# directly and sdl12-compat references the renamed SDL2 ones, so ordinary archive resolution
# pulls what is used - and pulling the rest would drag in SDL2's bootstraps twice over.
OOPS_SDL12_LDFLAGS := -Wl,--whole-archive $(OOPS_SDL12_LIB) -Wl,--no-whole-archive \
                      $(OOPS_SDL12_SDL2_LIB)

$(OOPS_SDL12_SDL2_LIB): $(OOPS_SDL_LIB) $(lastword $(MAKEFILE_LIST))
	@mkdir -p $(OOPS_SDL12_BUILD)
	@nm_tool=$$(command -v $(NM) 2>/dev/null || command -v llvm-nm 2>/dev/null || command -v nm); \
	 oc_tool=$$(command -v $(OBJCOPY) 2>/dev/null || command -v llvm-objcopy 2>/dev/null || command -v objcopy); \
	 if [ -z "$$nm_tool" ] || [ -z "$$oc_tool" ]; then \
	   echo "oops-sdl12: need nm and objcopy to rename SDL2's symbols, and one is missing" >&2; \
	   exit 1; \
	 fi; \
	 "$$nm_tool" --defined-only -g $(OOPS_SDL_LIB) 2>/dev/null \
	   | awk '$$3 ~ /^SDL_/ { print $$3, "IGNORE_THIS_VERSION_OF_" $$3 }' | sort -u \
	   > $(OOPS_SDL12_BUILD)/rename.map; \
	 cp $(OOPS_SDL_LIB) $@; \
	 "$$oc_tool" --redefine-syms=$(OOPS_SDL12_BUILD)/rename.map $@; \
	 echo "oops-sdl12: renamed $$(wc -l < $(OOPS_SDL12_BUILD)/rename.map) SDL2 symbols"
	@echo "oops-sdl12: $@"

$(OOPS_SDL12_LIB): $(OOPS_SDL12_UPSTREAM)/src/SDL12_compat.c $(lastword $(MAKEFILE_LIST)) \
                   | $(OOPS_SDL12_SDL2_LIB)
	@mkdir -p $(OOPS_SDL12_BUILD)
	@rm -f $@
	$(TARGET_CC) $(OOPS_SDL12_CFLAGS) -c -o $(OOPS_SDL12_BUILD)/SDL12_compat.o \
	    $(OOPS_SDL12_UPSTREAM)/src/SDL12_compat.c
	@ar_tool=$$(command -v $(AR) 2>/dev/null || command -v llvm-ar 2>/dev/null || command -v ar); \
	 "$$ar_tool" rcs $@ $(OOPS_SDL12_BUILD)/SDL12_compat.o
	@echo "oops-sdl12: $@"

.PHONY: sdl12-clean sdl12-upstream sdl12-upstream-clean
sdl12-clean:
	@rm -rf $(OOPS_SDL12_BUILD)
	@echo "sdl12-compat: removed build/"

sdl12-upstream:
	@$(OOPS_SDL12_DIR)/../../../common/upstream-fetch.sh \
	    "$$(sed -n 's/^UPSTREAM_KIND=//p' $(OOPS_SDL12_DIR)/upstream.lock)" \
	    "$$(sed -n 's/^UPSTREAM_URL=//p'  $(OOPS_SDL12_DIR)/upstream.lock)" \
	    "$$(sed -n 's/^UPSTREAM_REV=//p'  $(OOPS_SDL12_DIR)/upstream.lock)" \
	    "$(OOPS_SDL12_UPSTREAM)" "$(OOPS_SDL12_DIR)/patches"

sdl12-upstream-clean:
	@rm -rf $(OOPS_SDL12_UPSTREAM)
	@echo "sdl12-compat: removed upstream/"
