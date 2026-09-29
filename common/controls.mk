# The controls card (`common/controls_card.h`), for every title that carries a `controls.txt`.
#
# `common/app.mk` includes this after reading `app.env` and before resolving `OOPS_FEATURES`.
# With a `controls.txt` beside the title's Makefile it:
#
# - adds `controls_card.c` to the payload, with the table and the shared controller drawing
#   (`common/assets/controls/controller.png`) embedded by `#embed`, so nothing is packaged
#   and a changed table rebuilds through the object's own dependency file;
# - adds the features the card draws and reads with;
# - makes `oops_controls_entry` the entry point, which shows the card and then calls the
#   entry the title had - its own `ENTRY_POINT`, or app.mk's `<app>_start`.
#
# A title without the file is unchanged.
ifneq ($(wildcard controls.txt),)
OOPS_CONTROLS_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
OOPS_CONTROLS_NEXT := $(or $(ENTRY_POINT),$(subst -,_,$(or $(APP_NAME),$(notdir $(CURDIR))))_start)
ENTRY_POINT := oops_controls_entry

PAYLOAD_SRCS += $(OOPS_CONTROLS_DIR)/controls_card.c
OOPS_FEATURES += $(filter-out $(OOPS_FEATURES),display draw png input)
EXTRA_TARGET_CFLAGS += -DOOPS_CONTROLS_NEXT=$(OOPS_CONTROLS_NEXT) \
    '-DOOPS_CONTROLS_TITLE="$(subst ",,$(TITLE_NAME))"' \
    '-DOOPS_CONTROLS_TXT="$(abspath controls.txt)"' \
    '-DOOPS_CONTROLS_PNG="$(OOPS_CONTROLS_DIR)/assets/controls/controller.png"'
endif
