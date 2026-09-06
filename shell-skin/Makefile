# shell-skin - restyle the home screen by injecting a userscript into SceShellUI.
#
#   make check      compose the userscript on a host and verify its recipes and bounds
#   make skeleton   the compose logic compiles freestanding for the target, object only
#
# `all` is the host test and the skeleton, both of which build today. There is no `elf` target
# yet, and that is honest: the daemon's one hard step - reaching the running SceShellUI WebKit
# context to evaluate the script - is unproven, and gates on an obSCEne probe (can a payload
# inject into SceShellUI). The content is ready; the delivery is what the hardware has to answer.

OOPS_SDK ?= $(abspath ../../oops-sdk)
include $(OOPS_SDK)/oops-sdk.mk

CC ?= clang
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion \
          -Wstrict-prototypes -Wmissing-prototypes -O1 $(OOPS_SDK_INCLUDE)

TARGET_CC ?= clang
TARGET_CFLAGS ?= -std=c11 -Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion \
                 -Wstrict-prototypes -Wmissing-prototypes -Wvla \
                 -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib -fPIC \
                 -fno-stack-protector -fvisibility=hidden $(OOPS_SDK_INCLUDE)

BUILD ?= build

.PHONY: all check skeleton clean

all: $(BUILD)/shell_skin_selftest skeleton

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/shell_skin_selftest: shell-skin_selftest.c shell-skin.c shell-skin.h | $(BUILD)
	$(CC) $(CFLAGS) -o $@ shell-skin_selftest.c shell-skin.c

check: $(BUILD)/shell_skin_selftest
	@./$(BUILD)/shell_skin_selftest

skeleton: | $(BUILD)
	$(TARGET_CC) $(TARGET_CFLAGS) -c -o $(BUILD)/shell-skin.o shell-skin.c
	@echo "shell-skin skeleton: compiles freestanding for the target (object only)"

clean:
	rm -rf $(BUILD)
