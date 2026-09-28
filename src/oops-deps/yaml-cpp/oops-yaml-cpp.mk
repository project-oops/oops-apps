# yaml-cpp build integration.
#
#   OOPS_YAML ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/yaml-cpp)
#   include $(OOPS_YAML)/oops-yaml-cpp.mk
#   EXTRA_TARGET_LDFLAGS += $(OOPS_YAML_LDFLAGS)
#   PAYLOAD_EXTRA_DEPS   += $(OOPS_YAML_LIB)
#
# Every source in `src/` and `src/contrib/`, which is upstream's CMake list, as C++17 against the
# pinned libc++ with exceptions on: yaml-cpp reports a malformed document by throwing. Include
# `oops-libcxx.mk` first. `YAML_CPP_STATIC_DEFINE` drops the export decoration a shared build
# wants, as Torch's own CMake sets it.
ifndef OOPS_YAML_MK
OOPS_YAML_MK := 1

ifndef OOPS_YAML_DIR
OOPS_YAML_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
OOPS_YAML_UPSTREAM ?= $(OOPS_YAML_DIR)/upstream
OOPS_YAML_BUILD ?= $(OOPS_YAML_DIR)/build
OOPS_YAML_INCLUDE := -I$(OOPS_YAML_UPSTREAM)/include -DYAML_CPP_STATIC_DEFINE
OOPS_YAML_LIB := $(OOPS_YAML_BUILD)/libyaml-cpp.a
OOPS_YAML_LDFLAGS := $(OOPS_YAML_LIB)
OOPS_YAML_SRCS := $(wildcard $(OOPS_YAML_UPSTREAM)/src/*.cpp $(OOPS_YAML_UPSTREAM)/src/contrib/*.cpp)
OOPS_YAML_CXXFLAGS = -x c++ -std=c++17 -target x86_64-unknown-freebsd -ffreestanding -fno-builtin \
                     -nostdlib -nostdinc++ -nostdlibinc -fPIC -O2 -w -fexceptions -frtti \
                     $(OOPS_LIBCXX_INCLUDE) $(OOPS_YAML_INCLUDE) -I$(OOPS_YAML_UPSTREAM)/src \
                     $(OOPS_POSIX_INCLUDE) $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE)

$(OOPS_YAML_LIB): $(OOPS_YAML_SRCS) $(lastword $(MAKEFILE_LIST))
	@mkdir -p $(OOPS_YAML_BUILD)
	@rm -f $@
	@n=0; objs=""; for s in $(OOPS_YAML_SRCS); do n=$$((n+1)); o=$(OOPS_YAML_BUILD)/y$$n.o; \
	   $(TARGET_CC) $(OOPS_YAML_CXXFLAGS) -c -o "$$o" "$$s" || exit 1; objs="$$objs $$o"; done; \
	 echo "yaml-cpp: compiled $$n sources"; \
	 a=$$(command -v $(AR) 2>/dev/null || command -v ar); "$$a" rcs $@ $$objs
	@echo "yaml-cpp: $@"

.PHONY: yaml-cpp-clean
yaml-cpp-clean:
	@rm -rf $(OOPS_YAML_BUILD)

endif
