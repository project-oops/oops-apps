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
# **Two GL entry points, and they are the SDK's rather than this title's.**
#
# `libultraship`'s `gfx_opengl.cpp` calls 54 and `oops-gl` defines all but these. It is two and
# not four because the vertex-array-object pair at :725-726 sits inside
# `#if defined(__APPLE__) || defined(USE_OPENGLES)`, which this port does not compile - a symbol
# grep over a file with platform branches measures a build nobody runs. See README.md.
#
# `glRenderbufferStorageMultisample` is further behind a runtime `msaa_level`, so the one that is
# unconditionally load-bearing is `glBlitFramebuffer`.
SOH_GL_GAP := glBlitFramebuffer glRenderbufferStorageMultisample

# The third-party libraries upstream fetches, minus the Apple-only one. A name that already has a
# directory under `src/oops-deps/` is done; the rest are the vendoring queue.
SOH_DEPS := sdl2 zlib libzip stb nlohmann-json thread-pool tinyxml2 spdlog \
            stormlib libgfxd prism-processor

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
