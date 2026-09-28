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
# `make census-one S=<set> F=<path>` prints every error in one source, counted.

ifndef OOPS_CENSUS_MK
OOPS_CENSUS_MK := 1

define oops_census_set
@ok=0; bad=0; : > $(BUILD)/census/$(1).txt; \
 for f in $(CENSUS_$(1)_SRCS); do \
    if $(TARGET_CC) $(CENSUS_$(1)_FLAGS) -fsyntax-only "$$f" 2>$(BUILD)/census/e.txt; then \
        ok=$$((ok+1)); \
    else \
        bad=$$((bad+1)); \
        printf '%s\t%s\n' "$$f" \
          "$$(grep -a -m1 'error:' $(BUILD)/census/e.txt | sed 's|.*error: ||' | cut -c1-90)" \
          >> $(BUILD)/census/$(1).txt; \
    fi; \
 done; \
 printf '   %-16s %5s compile, %4s do not\n' "$(1)" "$$ok" "$$bad"

endef

.PHONY: census census-one
census:
	@mkdir -p $(BUILD)/census
	@echo "$(APP_NAME) census:"
	$(foreach s,$(CENSUS_SETS),$(call oops_census_set,$(s)))
	@cat $(foreach s,$(CENSUS_SETS),$(BUILD)/census/$(s).txt) | cut -f2 | sort | uniq -c \
	    | sort -rn | head -20

census-one:
	@$(TARGET_CC) $(CENSUS_$(S)_FLAGS) -fsyntax-only $(F) 2>&1 | grep 'error:' \
	    | sed 's|.*error: ||' | sort | uniq -c | sort -rn

endif
