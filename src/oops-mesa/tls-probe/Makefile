# tls-probe - the smallest title that carries a PT_TLS program header.
#
#   make elf      the module, which is the whole point: does it load?
#   make title    the title directory, for deploying next to the other apps
#
# Configuration & release formats loaded from app.env.
#
# # Why this app exists
#
# `mesa-probe` is refused by the loader at `sceSblAuthMgrAuthHeader`, and the only structural
# difference from every title that loads is a sixth program header: `PT_TLS`, which Mesa forces
# because its debug layer and shader backend use `__thread`. That is an inference from one
# sample. This app is the control - it differs from a loading title in that one property and
# nothing else - so the result separates "the loader refuses PT_TLS" from "the loader refuses
# something else about a 20 MB Mesa image".
#
# It answers `REQ-20260915T0001Z-8d72` on the SELFish bus, which asks whether the target honours
# `PT_TLS` in a title's own image, and which must not be resolved by adding the segment to the
# shared `link/native_eboot.ld` until this is known.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# No host test: the question is what the loader does with the module, and that is not something
# a host build can answer. A `make check` that passed here would be measuring nothing.

# Deliberately minimal. Every source beyond the two below would add program headers, imports or
# size, and each of those is a variable this control exists to hold still.
OOPS_MESA_DIR ?= $(abspath $(OOPS_APPS_ROOT)/../oops-mesa)

PAYLOAD_SRCS = tls_probe_main.c \
               $(OOPS_MESA_DIR)/src/runtime/emutls.c \
               $(OOPS_MESA_DIR)/src/runtime/threads.c \
               $(OOPS_SDK_DIR)/src/system/system.c \
               $(OOPS_SDK_DIR)/src/system/freestd.c \
               $(OOPS_SDK_DIR)/src/system/syscall.c \
               $(OOPS_SDK_DIR)/src/thread/thread.c

ENTRY_POINT := tls_probe_start

# No local link script. With `-femulated-tls` below there are no `STT_TLS` symbols left, so
# SELFish's stock `native_eboot.ld` links this cleanly and emits the same five program headers
# every title that loads has. That is the whole point of the second experiment: `local_tls.ld`
# was only ever needed because the thread-locals were native.

SYMBOLS_FILE := $(abspath build/imports.txt)

include $(OOPS_APPS_ROOT)/common/app.mk

# Emulated thread-local storage, which is the point of the second experiment. Appended after the
# include so app.mk's own flags are kept - setting it before replaced them.
#
# The console refuses a title carrying a `PT_TLS` segment; this app proved that, being 198 KB
# with nothing in it but two `__thread` variables and refused exactly as the 20 MB Mesa title
# was. `-femulated-tls` is the compiler's answer for a platform that cannot do native TLS: each
# thread-local becomes a `__emutls_v.*` control record in ordinary data, and a call to
# `__emutls_get_address` allocates per-thread storage on demand. No segment is emitted.
#
# If this loads *and* its values still survive, the whole problem is a build flag rather than
# eight patches to upstream Mesa.
TARGET_CFLAGS += -femulated-tls

# oops-mesa's `threads.c` and `emutls.c` include the platform's own `<pthread.h>` and `<errno.h>`,
# so they need the staged FreeBSD headers that D004 pins - without a sysroot the cross compiler
# reaches the build machine's Linux ones. This is the same sysroot mesa-probe uses; the probe
# takes only the headers, not any of Mesa.
TARGET_CFLAGS += --sysroot=$(OOPS_MESA_DIR)/toolchain/sysroot

$(SYMBOLS_FILE): $(BUILD)/$(APP_NAME).elf
	@if [ ! -f "$@" ]; then \
	  mkdir -p $(BUILD); \
	  OOPS_APPS_SYMBOLS=$(OOPS_APPS_ROOT)/common/symbols.txt \
	    bash $(OOPS_MESA_DIR)/tools/generate-imports.sh $< $@; \
	fi

.PHONY: imports
imports: $(BUILD)/$(APP_NAME).elf
	@mkdir -p $(BUILD)
	@OOPS_APPS_SYMBOLS=$(OOPS_APPS_ROOT)/common/symbols.txt \
	  bash $(OOPS_MESA_DIR)/tools/generate-imports.sh $< $(SYMBOLS_FILE)
