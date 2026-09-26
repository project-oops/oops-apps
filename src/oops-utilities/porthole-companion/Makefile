# porthole-companion - on-console utility to monitor, control and stage Porthole.
#
#   make check      host self-test: UI state transitions, off-screen rendering
#   make skeleton   freestanding target object compilation
#   make elf        freestanding target payload ELF (build/porthole-companion.elf)
#   make eboot      fSELF eboot.bin (build/eboot.bin)
#   make title      conforming title package (build/title/PORT00002)
#   make package    bundles porthole.elf into title package (dist/porthole-companion-title-prospero.zip)
#   make dist       stages conforming release artifact under dist/
#
# Configuration & release formats loaded from app.env

OOPS_APPS_ROOT ?= $(abspath ../../..)

OOPS_FEATURES := display draw input net http

# Synchronized payload dependency
PORTHOLE_PAYLOAD_DIR := $(OOPS_APPS_ROOT)/src/oops-payloads/porthole
PORTHOLE_PAYLOAD_ELF := $(PORTHOLE_PAYLOAD_DIR)/build/porthole.elf

# Share wire contract headers directly from porthole
EXTRA_CFLAGS        += -I$(PORTHOLE_PAYLOAD_DIR)
EXTRA_TARGET_CFLAGS += -I$(PORTHOLE_PAYLOAD_DIR)

# Host test sources
HOST_TEST_SRCS = porthole_companion_selftest.c porthole_companion.c \
                 $(OOPS_FEATURE_draw_SRCS) \
                 $(OOPS_SDK_DIR)/src/system/freestd.c

# Target payload sources
PAYLOAD_SRCS = porthole_companion.c porthole_companion_main.c

ENTRY_POINT := porthole_companion_start

include $(OOPS_APPS_ROOT)/common/app.mk

$(PORTHOLE_PAYLOAD_ELF): FORCE
	@$(MAKE) -C $(PORTHOLE_PAYLOAD_DIR) elf

.PHONY: package FORCE
package: title $(PORTHOLE_PAYLOAD_ELF)
	@mkdir -p $(BUILD)/title/$(TITLE_ID)/payload
	@cp $(PORTHOLE_PAYLOAD_ELF) $(BUILD)/title/$(TITLE_ID)/payload/porthole.elf
	@ZIP_OUT="$(CURDIR)/$(TITLE_ZIP_ARTIFACT)"; rm -f "$$ZIP_OUT"; \
	if command -v zip >/dev/null 2>&1; then \
	    ( cd $(BUILD)/title && zip -qr "$$ZIP_OUT" $(TITLE_ID) ); \
	elif python3 -c 'import zipfile' >/dev/null 2>&1; then \
	    ( cd $(BUILD)/title && python3 -m zipfile -c "$$ZIP_OUT" $(TITLE_ID) ); \
	elif tar --version 2>/dev/null | grep -qiE 'bsdtar|libarchive'; then \
	    ( cd $(BUILD)/title && tar -a -cf "$$ZIP_OUT" $(TITLE_ID) ); \
	fi
	@echo "porthole-companion: bundled $(PORTHOLE_PAYLOAD_ELF) into $(TITLE_ZIP_ARTIFACT)"

