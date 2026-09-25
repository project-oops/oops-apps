# Ship of Harkinian, through oops-sdk.
#
#   make survey   what this port still needs, measured against the tree that is checked out
#   make elf      the payload - NOT YET, see the state section of README.md
#
# **This title does not build yet, and the Makefile says so rather than pretending.** The fetch
# and the pin work; the sources do not compile against this toolchain until the SDK gains vertex
# array objects and the dependencies below are vendored. `make survey` is what is useful today:
# it measures the gap against the checked-out tree, so the number moves as the work lands instead
# of being re-guessed.
#
# The order of work is in README.md. Read it before adding to this file.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# The fetch runs while this file is read - see `common/upstream.mk`. It is included explicitly
# because the survey below names files under `upstream/`, and a title that does that has to ask
# for the fetch before the lists that use it.
include $(OOPS_APPS_ROOT)/common/upstream.mk

# --- what is still missing --------------------------------------------------
#
# **The GL gap, and it is the SDK's rather than this title's.**
#
# `libultraship`'s `gfx_opengl.cpp` calls 54 entry points and `oops-gl` defines all but the first
# two below. The vertex-array family is *not* from that file - it sits there inside
# `#if defined(__APPLE__) || defined(USE_OPENGLES)`, which this port does not compile. It is here
# because **ImGui's GL3 backend uses vertex array objects unconditionally on desktop GL**, and
# libultraship links that backend. Grepping the port measured the port; compiling the link
# measured the link. See README.md.
#
# `glRenderbufferStorageMultisample` is further behind a runtime `msaa_level`, so the one that is
# unconditionally load-bearing from `gfx_opengl.cpp` is `glBlitFramebuffer`.
SOH_GL_GAP := glBlitFramebuffer glRenderbufferStorageMultisample \
              glGenVertexArrays glBindVertexArray glDeleteVertexArrays glGetStringi

# The third-party libraries upstream fetches, minus the Apple-only one. A name that already has a
# directory under `src/oops-deps/` is done; the rest are the vendoring queue.
#
# `imgui` was missing from this list until a compile survey found it: it is declared and used
# inside the `libultraship` submodule, so the grep that built this list - run over the
# superproject - could not see it.
SOH_DEPS := sdl2 zlib libzip stb nlohmann-json thread-pool tinyxml2 spdlog \
            stormlib libgfxd prism-processor imgui

# --- generated headers ------------------------------------------------------
#
# CMake hands libultraship two things this build has to produce another way. Both are generated
# rather than checked in, for the reason each script gives: they are derived from files upstream
# owns, and a stale copy of either is wrong in a way that compiles.
#
# `ship/install_config.h` is the exception - it is a *decision* rather than a derivation, so it
# lives in `shim/include/` as ordinary source.
# **`-include`, because upstream delivers these on the command line.** CMake passes the CVAR names
# as `add_compile_definitions`, so no source includes a header for them - `Controller.cpp:69`
# simply writes `CVAR_PREFIX_CONTROLLERS ".Port%d.HasConfig"` and expects the macro to be there.
# Reproducing that as twenty-two `-D` flags would mean quoting each value through make and the
# shell; one force-include of a generated header is the same thing with the quoting done once, in
# C, where it belongs.
# --- what the build selects ---------------------------------------------------
#
# `ENABLE_OPENGL` is how libultraship picks a renderer. Its CMake sets one of `ENABLE_OPENGL`,
# `ENABLE_DX11`, `ENABLE_DX12` or the Metal path, and `Gui.cpp` includes the matching ImGui
# backend behind that `#ifdef`. Without it the file compiles right up to
# `ImGui_ImplSDL2_InitForOpenGL` and then does not know what that is - which reads like a missing
# header and is really an unselected renderer.
#
# There is one renderer on this target, so this is not a choice so much as a statement.
SOH_DEFS := -DENABLE_OPENGL

SOH_GEN_DIR := $(CURDIR)/build/gen
SOH_CVARS_H := $(SOH_GEN_DIR)/ship/cvars_generated.h
SOH_GEN_INCLUDE := -I$(SOH_GEN_DIR) -I$(CURDIR)/shim/include \
                   -include ship/cvars_generated.h

$(SOH_CVARS_H): $(CURDIR)/gen-cvars.sh \
                upstream/CMake/soh-cvars.cmake upstream/CMake/lus-cvars.cmake \
                upstream/libultraship/cmake/cvars.cmake
	@sh $(CURDIR)/gen-cvars.sh $(CURDIR)/upstream $@

.PHONY: generated
generated: $(SOH_CVARS_H)

.PHONY: survey
survey:
	@echo "ship-of-harkinian: upstream $(UPSTREAM_REF) @ $(UPSTREAM_REV)"
	@printf '\n-- submodules\n'
	@for m in libultraship ZAPDTR OTRExporter; do \
	    if [ -n "$$(ls -A upstream/$$m 2>/dev/null)" ]; then s=present; else s="MISSING"; fi; \
	    printf '   %-14s %s\n' "$$m" "$$s"; \
	done
	@printf '\n-- source volume to compile\n'
	@for p in soh/soh libultraship/src ZAPDTR OTRExporter; do \
	    printf '   %-18s c:%-5s cpp:%-5s\n' "$$p" \
	        "$$(find upstream/$$p -name '*.c' 2>/dev/null | wc -l)" \
	        "$$(find upstream/$$p -name '*.cpp' 2>/dev/null | wc -l)"; \
	done
	@printf '\n-- GL entry points oops-gl still lacks\n'
	@gl=$(OOPS_SDK_DIR)/src/gl; missing=0; \
	 for s in $(SOH_GL_GAP); do \
	    if grep -rqE "^[A-Za-z_][A-Za-z0-9_ *]*\b$$s\(" $$gl/*.c 2>/dev/null; then \
	        printf '   %-34s defined\n' "$$s"; \
	    else \
	        printf '   %-34s MISSING\n' "$$s"; missing=$$((missing+1)); \
	    fi; \
	 done; printf '   -> %s of %s still to add\n' "$$missing" "$(words $(SOH_GL_GAP))"
	@printf '\n-- dependencies\n'
	@have=0; need=0; \
	 for d in $(SOH_DEPS); do \
	    if [ -d $(OOPS_APPS_ROOT)/src/oops-deps/$$d ]; then \
	        printf '   %-18s vendored\n' "$$d"; have=$$((have+1)); \
	    else \
	        printf '   %-18s to vendor\n' "$$d"; need=$$((need+1)); \
	    fi; \
	 done; printf '   -> %s vendored, %s to go\n' "$$have" "$$need"

# `make` with no target should say what the state is rather than fail obscurely.
.DEFAULT_GOAL := survey
