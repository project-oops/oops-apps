# cimgui build integration.
#
#   OOPS_CIMGUI ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/cimgui)
#   include $(OOPS_CIMGUI)/oops-cimgui.mk
#   EXTRA_TARGET_CFLAGS  += $(OOPS_CIMGUI_INCLUDE)
#   EXTRA_TARGET_LDFLAGS += $(OOPS_CIMGUI_LDFLAGS)
#   PAYLOAD_EXTRA_DEPS   += $(OOPS_CIMGUI_LIB)
#
# `cimgui.cpp` alone, as Ghostship's CMakeLists.txt builds it (`add_library(cimgui STATIC
# cimgui.cpp)`), over the ImGui in `../imgui`, which must be included first. Consumers compile
# with `CIMGUI_DEFINE_ENUMS_AND_STRUCTS`, which `OOPS_CIMGUI_INCLUDE` carries. It is linked whole,
# as upstream links it: C code reaches it only through function pointers ImGui never names.
ifndef OOPS_CIMGUI_MK
OOPS_CIMGUI_MK := 1

ifndef OOPS_CIMGUI_DIR
OOPS_CIMGUI_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
OOPS_CIMGUI_UPSTREAM ?= $(OOPS_CIMGUI_DIR)/upstream
OOPS_CIMGUI_BUILD ?= $(OOPS_CIMGUI_DIR)/build
OOPS_CIMGUI_INCLUDE := -I$(OOPS_CIMGUI_UPSTREAM) -DCIMGUI_DEFINE_ENUMS_AND_STRUCTS
OOPS_CIMGUI_LIB := $(OOPS_CIMGUI_BUILD)/libcimgui.a
OOPS_CIMGUI_LDFLAGS := -Wl,--whole-archive $(OOPS_CIMGUI_LIB) -Wl,--no-whole-archive
OOPS_CIMGUI_SRCS := $(OOPS_CIMGUI_UPSTREAM)/cimgui.cpp
# `cimgui.cpp` includes its own submodule's `./imgui/imgui.h`, relative to itself, which no `-I`
# can redirect - and that copy lacks `../imgui`'s patch, whose one header change is
# `imconfig.h` (`ImTextureID` is `void *` there, `ImU64` by default). `IMGUI_USER_CONFIG` is
# ImGui's hook for a configuration kept elsewhere, so cimgui compiles against the same one: the
# two disagreeing left every `ImTextureID` function cimgui calls unresolved.
OOPS_CIMGUI_CXXFLAGS = $(OOPS_IMGUI_CXXFLAGS) -I$(OOPS_CIMGUI_UPSTREAM) \
                       '-DIMGUI_USER_CONFIG="$(OOPS_IMGUI_UPSTREAM)/imconfig.h"'

$(OOPS_CIMGUI_LIB): $(OOPS_CIMGUI_SRCS) $(lastword $(MAKEFILE_LIST))
	@mkdir -p $(OOPS_CIMGUI_BUILD)
	@rm -f $@
	$(TARGET_CC) -x c++ $(OOPS_CIMGUI_CXXFLAGS) -c -o $(OOPS_CIMGUI_BUILD)/cimgui.o $(OOPS_CIMGUI_SRCS)
	@a=$$(command -v $(AR) 2>/dev/null || command -v ar); "$$a" rcs $@ $(OOPS_CIMGUI_BUILD)/cimgui.o
	@echo "cimgui: $@"

.PHONY: cimgui-clean
cimgui-clean:
	@rm -rf $(OOPS_CIMGUI_BUILD)

endif
