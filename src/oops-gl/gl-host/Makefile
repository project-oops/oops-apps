# gl-host - oops-gl as the GL for an ordinary desktop program.
#
#   make            build liboopsGL.so
#   make clean
#
# Host-only and standalone: this builds a Linux shared library for LD_PRELOAD, not a console
# payload, so it does not go through common/app.mk.
#
# Usage, from wherever the program under test lives:
#
#   OOPS_GL_HOST_DUMP=/tmp/shot LD_PRELOAD=/path/to/liboopsGL.so ./the-program

OOPS_APPS_ROOT ?= $(abspath ../../..)
OOPS_SDK_DIR   ?= $(abspath $(OOPS_APPS_ROOT)/../oops-sdk)

CC      ?= clang
BUILD   ?= build
TARGET   = $(BUILD)/liboopsGL.so

# The GL layer entire, plus the three pieces of the SDK it leans on. The display is not among
# them: the shim supplies its own, because the real one talks to hardware that is not here.
SRCS = $(wildcard $(OOPS_SDK_DIR)/src/gl/*.c) \
       $(OOPS_SDK_DIR)/src/math/math.c \
       $(OOPS_SDK_DIR)/src/system/system.c \
       $(OOPS_SDK_DIR)/src/system/freestd.c \
       gl_host_shim.c

# Default visibility, unlike the payload builds of these sources: a program already compiled
# against GL must find these symbols rather than the system's.
CFLAGS  = -std=c11 -O1 -g -fPIC -Wall -Wextra \
          -DOOPS_HOST_BUILD -DOOPS_TARGET=3 \
          -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L \
          -I$(OOPS_SDK_DIR)/include -I$(OOPS_SDK_DIR)
LDFLAGS = -shared
LDLIBS  = -ldl -lm

.PHONY: all clean
all: $(TARGET)

$(TARGET): $(SRCS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(SRCS) $(LDLIBS)
	@echo "gl-host: $@"

clean:
	rm -rf $(BUILD)
