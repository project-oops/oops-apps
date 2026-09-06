OOPS_SDK ?= $(abspath ../../oops-sdk)
include $(OOPS_SDK)/oops-sdk.mk

CC ?= clang
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion \
          -Wstrict-prototypes -Wmissing-prototypes -O1 -DOOPS_HOST_BUILD \

TARGET_CC ?= clang
TARGET_CFLAGS ?= -std=c11 -Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion \
                 -Wstrict-prototypes -Wmissing-prototypes -Wvla \
                 -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib -fPIC \
                 -fno-stack-protector -fvisibility=hidden \
                 $(OOPS_SDK_INCLUDE)

BUILD ?= build

.PHONY: all check skeleton elf clean

all: $(BUILD)/injector_selftest skeleton

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/injector_selftest: injector_selftest.c $(SDK_HOST_SRCS) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ injector_selftest.c $(SDK_HOST_SRCS)


skeleton: | $(BUILD)
	$(TARGET_CC) $(TARGET_CFLAGS) -c -o $(BUILD)/injector.o injector.c
	@echo "injector skeleton: compiles freestanding for the target (object only, not linked)"

INJECTOR_PAYLOAD_SRC := injector.c \
                        $(OOPS_SDK_DIR)/src/system/freestd.c \
                        $(OOPS_SDK_DIR)/src/system/syscall.c \
                        $(OOPS_SDK_DIR)/src/system/krw.c \
                        $(OOPS_SDK_DIR)/src/system/procctl.c \
                        $(OOPS_SDK_DIR)/src/system/loader.c \
                        $(OOPS_SDK_DIR)/src/system/target.c \
                        $(OOPS_SDK_DIR)/src/system/inject.c

$(BUILD)/injector.elf: $(INJECTOR_PAYLOAD_SRC) | $(BUILD)
	$(TARGET_CC) $(TARGET_CFLAGS) -fuse-ld=lld -shared -Wl,-e,injector_start \
	    -Wl,--unresolved-symbols=ignore-all -Wl,-z,noexecstack \
	    -Wl,-z,max-page-size=0x4000 -Wl,-z,common-page-size=0x4000 \
	    -o $@ $(INJECTOR_PAYLOAD_SRC)

elf: $(BUILD)/injector.elf

clean:
	rm -rf $(BUILD)
