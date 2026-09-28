# `make census`: which of a title's sources compile for the target, set by set.
#
# Included after `common/app.mk`. A title whose program is several components - an engine, a
# renderer, a game library - names each as a set:
#
#   CENSUS_SETS         := engine ref_gl client
#   CENSUS_engine_SRCS  := <the sources>
#   CENSUS_engine_FLAGS  = <the flags they compile with, C or C++>
#
# and `make census` compiles every source with `-fsyntax-only`, prints a line per set, and
# writes each failing source with its first error to `$(BUILD)/census/<set>.txt`. The top
# twenty first-errors across every set follow, since one missing header usually accounts for
# most of a set. `-fsyntax-only` does not link: a clean census is not a working payload.
#
# Each source is its own target, so `make -j16 census` compiles sixteen at once - the
# libultraship titles have over a thousand sources, which is most of an hour one at a time.
# Every run starts from nothing: a flag changed since the last one would otherwise leave its
# answers standing.
#
# `make census-one S=<set> F=<path>` prints every error in one source, counted.

ifndef OOPS_CENSUS_MK
OOPS_CENSUS_MK := 1

CENSUS_DIR := $(BUILD)/census

# A source's check is `<set>/<its absolute path>.chk`; a failure leaves a `.bad` beside it
# holding the line `<set>.txt` gathers.
census_chks = $(patsubst /%,$(CENSUS_DIR)/$(1)/%.chk,$(sort $(abspath $(CENSUS_$(1)_SRCS))))

define oops_census_rule
$(CENSUS_DIR)/$(1)/%.chk: /%
	@mkdir -p $$(@D)
	@if $$(TARGET_CC) $$(CENSUS_$(1)_FLAGS) -fsyntax-only $$< 2>$$@.e; then :; else \
	    printf '%s\t%s\n' "$$<" \
	      "$$$$(grep -a -m1 'error:' $$@.e | sed 's|.*error: ||' | cut -c1-90)" > $$@.bad; \
	 fi; rm -f $$@.e; touch $$@
endef
$(foreach s,$(CENSUS_SETS),$(eval $(call oops_census_rule,$(s))))

define oops_census_tally
@mkdir -p $(CENSUS_DIR)/$(1); \
 find $(CENSUS_DIR)/$(1) -name '*.bad' -exec cat {} + | sort > $(CENSUS_DIR)/$(1).txt; \
 bad=$$(wc -l < $(CENSUS_DIR)/$(1).txt); \
 printf '   %-16s %5s compile, %4s do not\n' "$(1)" \
     "$$(($(words $(call census_chks,$(1))) - bad))" "$$bad"

endef

.PHONY: census census-collect census-one
census:
	@rm -rf $(CENSUS_DIR) && mkdir -p $(CENSUS_DIR)
	@$(MAKE) --no-print-directory census-collect

census-collect: $(foreach s,$(CENSUS_SETS),$(call census_chks,$(s)))
	@echo "$(APP_NAME) census:"
	$(foreach s,$(CENSUS_SETS),$(call oops_census_tally,$(s)))
	@cat $(foreach s,$(CENSUS_SETS),$(CENSUS_DIR)/$(s).txt) | cut -f2 | sort | uniq -c \
	    | sort -rn | head -20

census-one:
	@$(TARGET_CC) $(CENSUS_$(S)_FLAGS) -fsyntax-only $(F) 2>&1 | grep 'error:' \
	    | sed 's|.*error: ||' | sort | uniq -c | sort -rn

endif
