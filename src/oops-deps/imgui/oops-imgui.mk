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
# # Five of the seven sources compile; the GL3 backend does not, yet
#
# `imgui_impl_opengl3.cpp` names ten things `oops-gl` does not have: `glGenVertexArrays`,
# `glBindVertexArray`, `glDeleteVertexArrays`, `glGetStringi`, and the enums `GL_MAJOR_VERSION`,
# `GL_MINOR_VERSION`, `GL_NUM_EXTENSIONS`, `GL_VERTEX_ARRAY_BINDING`, `GL_PIXEL_UNPACK_BUFFER` and
# `GL_PIXEL_UNPACK_BUFFER_BINDING`.
#
# **That is a correction to what this title's README used to say.** It recorded the vertex-array
# calls as *not* needed, because in `libultraship`'s own sources they appear only inside
# `#if defined(__APPLE__) || defined(USE_OPENGLES)`. True, and beside the point: ImGui's GL3
# backend uses them unconditionally on desktop GL, and libultraship links that backend. The
# earlier answer came from grepping the port; this one came from compiling it.
#
# Vertex array objects are real work in `oops-gl` - they capture attribute state, they are not a
# handle to wrap - so they are their own job rather than something to bolt on here.
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
#
# `imgui_demo.cpp` is one of the five. It is the demo window, which a shipped title never opens -
# but libultraship lists it, and `ImGui::ShowDemoWindow` is reachable from the runtime's own debug
# menu, so leaving it out would turn a menu entry into a link-time hole that a payload link does
# not report.
OOPS_IMGUI_SRCS := \
    $(OOPS_IMGUI_UPSTREAM)/imgui.cpp \
    $(OOPS_IMGUI_UPSTREAM)/imgui_demo.cpp \
    $(OOPS_IMGUI_UPSTREAM)/imgui_draw.cpp \
    $(OOPS_IMGUI_UPSTREAM)/imgui_tables.cpp \
    $(OOPS_IMGUI_UPSTREAM)/imgui_widgets.cpp \
    $(OOPS_IMGUI_UPSTREAM)/backends/imgui_impl_opengl3.cpp \
    $(OOPS_IMGUI_UPSTREAM)/backends/imgui_impl_sdl2.cpp

# `IMGUI_IMPL_OPENGL_LOADER_CUSTOM` with no loader header behind it: `imgui_impl_opengl3.cpp`
# otherwise picks a loader (glad, glew, gl3w) and includes it. `oops-gl` *is* the GL here and its
# entry points are ordinary linked symbols, so there is nothing to load at run time - the define
# tells the backend to use whatever `<GL/gl.h>` already declared, which is ours.
#
# `IMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS` is upstream's own option for a platform with no shell to
# open a path in. Without it `imgui.cpp` reaches for `<sys/wait.h>`, `fork` and `xdg-open`, none of
# which exist on this console - and the feature behind them is "click a link in the demo window",
# which has nowhere to go here anyway. A flag upstream provides beats a patch we would rebase.
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
