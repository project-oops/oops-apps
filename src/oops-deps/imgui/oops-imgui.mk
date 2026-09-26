# Dear ImGui build integration.
#
#   OOPS_IMGUI ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/imgui)
#   include $(OOPS_IMGUI)/oops-imgui.mk
#   EXTRA_TARGET_CFLAGS += $(OOPS_IMGUI_INCLUDE)
#   EXTRA_LDFLAGS       += $(OOPS_IMGUI_LDFLAGS)
#
# The immediate-mode UI libultraship builds its debug and configuration interface on. Needs SDL
# and oops-gl, so include `oops-sdl.mk` and the SDK's GL feature first.
#
# `imgui_impl_opengl3.cpp` uses vertex array objects, `glGetStringi` and the GL3 version and
# extension enums unconditionally on desktop GL, so it needs them from `oops-gl`.
ifndef OOPS_IMGUI_DIR
OOPS_IMGUI_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
OOPS_IMGUI_UPSTREAM ?= $(OOPS_IMGUI_DIR)/upstream
OOPS_IMGUI_BUILD ?= $(OOPS_IMGUI_DIR)/build

OOPS_IMGUI_INCLUDE := -I$(OOPS_IMGUI_UPSTREAM) -I$(OOPS_IMGUI_UPSTREAM)/backends
OOPS_IMGUI_LIB := $(OOPS_IMGUI_BUILD)/libimgui.a
OOPS_IMGUI_LDFLAGS := $(OOPS_IMGUI_LIB)

# Upstream's `target_sources` from `libultraship/cmake/dependencies/common.cmake:21` and `:30` -
# the five core files and the two backends libultraship selects, not every backend in the tree.
# `imgui_demo.cpp` stays: `ImGui::ShowDemoWindow` is reachable from the runtime's debug menu,
# and a payload link does not report the missing symbol.
OOPS_IMGUI_SRCS := \
    $(OOPS_IMGUI_UPSTREAM)/imgui.cpp \
    $(OOPS_IMGUI_UPSTREAM)/imgui_demo.cpp \
    $(OOPS_IMGUI_UPSTREAM)/imgui_draw.cpp \
    $(OOPS_IMGUI_UPSTREAM)/imgui_tables.cpp \
    $(OOPS_IMGUI_UPSTREAM)/imgui_widgets.cpp \
    $(OOPS_IMGUI_UPSTREAM)/backends/imgui_impl_opengl3.cpp \
    $(OOPS_IMGUI_UPSTREAM)/backends/imgui_impl_sdl2.cpp

# `IMGUI_IMPL_OPENGL_LOADER_CUSTOM` with no loader header: `oops-gl` entry points are linked
# symbols, so the backend uses what `<GL/gl.h>` declares instead of glad, glew or gl3w.
#
# `IMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS` is upstream's option for a platform with no shell;
# without it `imgui.cpp` wants `<sys/wait.h>`, `fork` and `xdg-open`.
OOPS_IMGUI_DEFS := -DIMGUI_IMPL_OPENGL_LOADER_CUSTOM -DIMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS

OOPS_IMGUI_CXXFLAGS = -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib \
                      -fPIC -O2 -w -std=c++20 \
                      $(OOPS_IMGUI_INCLUDE) $(OOPS_IMGUI_DEFS) -include GL/gl.h \
                      $(OOPS_SDL_INCLUDE) $(OOPS_SDL_PREFIX_INCLUDE) \
                      $(OOPS_LIBCXX_INCLUDE) \
                      $(OOPS_POSIX_INCLUDE) $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE)

$(OOPS_IMGUI_LIB): $(OOPS_IMGUI_SRCS) $(lastword $(MAKEFILE_LIST)) | $(OOPS_SDL_PREFIX_STAMP)
	@mkdir -p $(OOPS_IMGUI_BUILD)
	@rm -f $@
	@n=0; objs=""; for s in $(OOPS_IMGUI_SRCS); do n=$$((n+1)); o=$(OOPS_IMGUI_BUILD)/i$$n.o; \
	   $(TARGET_CXX) $(OOPS_IMGUI_CXXFLAGS) -c -o "$$o" "$$s" || exit 1; objs="$$objs $$o"; done; \
	 echo "ImGui: compiled $$n sources"; \
	 a=$$(command -v $(AR) 2>/dev/null || command -v ar); "$$a" rcs $@ $$objs
	@echo "ImGui: $@"

.PHONY: imgui-clean
imgui-clean:
	@rm -rf $(OOPS_IMGUI_BUILD)
