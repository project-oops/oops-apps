# Porthole - the target half of the capture-and-input path.
#
# The host half is Prosperous (pros-core::watch, pros-link); this is the payload that runs on
# the hardware, streams the framebuffer out and takes controller state in over its own
# sockets. It builds on its own, the way every app here does.
#
#   make check      the wire contract round-trips on a host, and the scaffold is honestly one
#   make skeleton   the payload half compiles freestanding for the target, object only
#   make elf        the plain-ELF payload an on-console loader (elfldr) runs
#   make dist       stage what CI publishes, under dist/ - for Porthole, the payload ELF
#
# The base layer it needs before it can do anything - a freestanding runtime, the syscall
# interface, kernel read/write - is consumed directly from oops-sdk via oops-sdk.mk.

OOPS_SDK ?= $(abspath ../../oops-sdk)
include $(OOPS_SDK)/oops-sdk.mk

CC ?= clang
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion \
          -Wstrict-prototypes -Wmissing-prototypes -O1 -DPORTHOLE_HOST_BUILD \
          $(OOPS_SDK_INCLUDE)

# The target skeleton cross-compiles for freebsd, which needs clang specifically - `cc` on a
# build host is often gcc, and gcc does not take `-target`. Kept separate from CC so the host
# check can use whatever `cc` is while the skeleton always uses clang.
TARGET_CC ?= clang

# Freestanding flags for the target-side build, matching the SDK's own. The skeleton object is
# compiled but not linked: a real payload needs the CRT and net wiring, which is the work the
# encoder go/no-go gates - this proves only that the skeleton is sound freestanding C.
TARGET_CFLAGS ?= -std=c11 -Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion \
                 -Wstrict-prototypes -Wmissing-prototypes -Wvla \
                 -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib -fPIC \
                 -fno-stack-protector -fvisibility=hidden \
                 $(OOPS_SDK_INCLUDE)

# The struct-taking encoder calls - QueryMemorySize, CreateEncoder, SetInputFrame, GetAuData -
# pass parameter layouts not yet confirmed against the platform, which D300 reserved for M2. They
# are compiled in only on request, so a first hardware run exercises the sockets and the template
# stream and nothing that could corrupt the stack. Once the layouts are confirmed:
#
#   make elf PORTHOLE_ENCODER_SESSION=1
ifdef PORTHOLE_ENCODER_SESSION
TARGET_CFLAGS += -DPORTHOLE_ENCODER_SESSION
endif

BUILD ?= build
DIST ?= dist

.PHONY: all check skeleton elf dist clean

all: $(BUILD)/porthole_selftest skeleton elf

$(BUILD):
	mkdir -p $(BUILD)

# The SDK's net layer comes in even on the host: Porthole's sockets are `oops_*` on both sides of
# the build now, and on a host those compile to an honest -1 (the server loop is never reached
# there anyway, because the encoder refuses first).
$(BUILD)/porthole_selftest: porthole_selftest.c porthole.c porthole.h | $(BUILD)
	$(CC) $(CFLAGS) -o $@ porthole_selftest.c porthole.c $(OOPS_SDK_DIR)/src/net/net.c

# The gate: the wire contract round-trips and the scaffold is honestly a scaffold.
check: $(BUILD)/porthole_selftest
	@./$(BUILD)/porthole_selftest

# Proves the payload half compiles freestanding for the target, object-only (no CRT/link yet).
skeleton: | $(BUILD)
	$(TARGET_CC) $(TARGET_CFLAGS) -c -o $(BUILD)/porthole.o porthole.c
	@echo "porthole skeleton: compiles freestanding for the target (object only, not linked)"

PORTHOLE_PAYLOAD_SRC := porthole.c porthole_main.c \
                        $(OOPS_SDK_DIR)/src/system/freestd.c \
                        $(OOPS_SDK_DIR)/src/system/syscall.c \
                        $(OOPS_SDK_DIR)/src/system/krw.c \
                        $(OOPS_SDK_DIR)/src/memory/memory.c \
                        $(OOPS_SDK_DIR)/src/time/time.c \
                        $(OOPS_SDK_DIR)/src/net/net.c \
                        $(OOPS_SDK_DIR)/src/display.c \
                        $(OOPS_SDK_DIR)/src/agc/agc_display.c \
                        $(OOPS_SDK_DIR)/src/agc/agc_tiler.c \
                        $(OOPS_SDK_DIR)/src/gnm/gnm_display.c

# The plain-ELF target payload executed by elfldr on the console.
$(BUILD)/porthole.elf: $(PORTHOLE_PAYLOAD_SRC) porthole.h | $(BUILD)
	$(TARGET_CC) $(TARGET_CFLAGS) -fuse-ld=lld -shared -Wl,-e,porthole_start \
	    -Wl,--unresolved-symbols=ignore-all -Wl,-z,noexecstack \
	    -Wl,-z,max-page-size=0x4000 -Wl,-z,common-page-size=0x4000 \
	    -o $@ $(PORTHOLE_PAYLOAD_SRC)

elf: $(BUILD)/porthole.elf

# The release contract (see bin/oops-apps): put what ships under dist/. Porthole ships one
# thing, the plain-ELF payload, qualified by target generation per CONVENTIONS.md section 2.
dist: $(BUILD)/porthole.elf
	@mkdir -p $(DIST)
	cp $(BUILD)/porthole.elf $(DIST)/porthole-$(TARGET).elf
	@$(call oops_verify_dist,$(DIST))

clean:
	rm -rf $(BUILD) $(DIST)
