OOPS_SDK ?= $(abspath ../../oops-sdk)
include $(OOPS_SDK)/oops-sdk.mk

CC ?= clang
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion \
          -Wstrict-prototypes -Wmissing-prototypes -O1 -DOOPS_HOST_BUILD \
          $(OOPS_SDK_INCLUDE)

TARGET_CC ?= clang
TARGET_CFLAGS ?= -std=c11 -Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion \
                 -Wstrict-prototypes -Wmissing-prototypes -Wvla \
                 -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib -fPIC \
                 -fno-stack-protector -fvisibility=hidden \
                 $(OOPS_SDK_INCLUDE)

BUILD ?= build
DIST ?= dist

.PHONY: all check skeleton elf dist clean

all: $(BUILD)/pltauth_patch_selftest elf

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/pltauth_patch_selftest: pltauth_patch_selftest.c | $(BUILD)
	$(CC) $(CFLAGS) -o $@ pltauth_patch_selftest.c $(OOPS_SDK_DIR)/src/system/freestd.c

check: $(BUILD)/pltauth_patch_selftest
	./$(BUILD)/pltauth_patch_selftest

skeleton: | $(BUILD)
	$(TARGET_CC) $(TARGET_CFLAGS) -c -o $(BUILD)/pltauth_patch.o pltauth_patch.c
	@echo "pltauth-patch skeleton: compiles freestanding for the target (object only)"

PLTAUTH_PAYLOAD_SRC := pltauth_patch.c \
                       $(OOPS_SDK_DIR)/src/system/freestd.c \
                       $(OOPS_SDK_DIR)/src/system/syscall.c \
                       $(OOPS_SDK_DIR)/src/system/krw.c

$(BUILD)/pltauth-patch.elf: $(PLTAUTH_PAYLOAD_SRC) | $(BUILD)
	$(TARGET_CC) $(TARGET_CFLAGS) -fuse-ld=lld -shared -Wl,-e,pltauth_patch_start \
	    -Wl,--unresolved-symbols=ignore-all -Wl,-z,noexecstack \
	    -Wl,-z,max-page-size=0x4000 -Wl,-z,common-page-size=0x4000 \
	    -o $@ $(PLTAUTH_PAYLOAD_SRC)

elf: $(BUILD)/pltauth-patch.elf

# The release contract (see bin/oops-apps): put what ships under dist/.
# Staged artifacts explicitly encode target generation per CONVENTIONS.md section 2.
dist: $(BUILD)/pltauth-patch.elf
	@mkdir -p $(DIST)
	cp $(BUILD)/pltauth-patch.elf $(DIST)/pltauth-patch-$(TARGET).elf
	@$(call oops_verify_dist,$(DIST))

clean:
	rm -rf $(BUILD) $(DIST)

