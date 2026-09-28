# A title that is pinned but not yet armed: its survey, and the guard that keeps an unarmed
# `make title` from packaging a stale payload.
#
# Included after `common/app.mk`. The title sets, before the include:
#
#   SCAFFOLD_ARM      the name of its arming variable, `GHST_ARMED` and the like
#   SCAFFOLD_DEPS     the `src/oops-deps/` names its link needs; the survey says which exist
#   SCAFFOLD_SUPPLY   what the player copies in, and where - the same sentence the title shows
#                     on screen when it is missing
#
# and arms its source lists with `ifeq ($(GHST_ARMED),1)`. Until it does, `make` runs the
# survey, `all` builds the host selftest and no payload, and `make title` refuses.
#
# `survey` is a double-colon rule, so a title adds its own lines - a source count, a
# submodule check - with a `survey::` of its own after this include.

ifndef OOPS_SCAFFOLD_MK
OOPS_SCAFFOLD_MK := 1

SCAFFOLD_ARMED := $($(SCAFFOLD_ARM))

.PHONY: survey
survey::
	@echo "$(APP_NAME) ($(TITLE_ID)): upstream $(UPSTREAM_REF) @ $(UPSTREAM_REV)"
	@for n in $(UPSTREAM_EXTRA); do \
	    ( . ./upstream-$$n.lock; \
	      printf '   + upstream-%-10s %s @ %s\n' "$$n" "$$UPSTREAM_REF" "$$UPSTREAM_REV" ); \
	 done
	@printf '\n-- source volume, every C and C++ file in each origin\n'
	@for d in upstream $(addprefix upstream-,$(UPSTREAM_EXTRA)); do \
	    printf '   %-18s c:%-6s c++:%-6s\n' "$$d" \
	        "$$(find $$d -name '*.c' 2>/dev/null | wc -l)" \
	        "$$(find $$d \( -name '*.cpp' -o -name '*.cc' \) 2>/dev/null | wc -l)"; \
	 done
	@printf '\n-- the player supplies\n   %s\n' "$(SCAFFOLD_SUPPLY)"
	@printf '\n-- dependencies\n'
	@have=0; need=0; \
	 for d in $(SCAFFOLD_DEPS); do \
	    if [ -d $(OOPS_APPS_ROOT)/src/oops-deps/$$d ]; then \
	        printf '   %-18s vendored\n' "$$d"; have=$$((have+1)); \
	    else \
	        printf '   %-18s TO VENDOR\n' "$$d"; need=$$((need+1)); \
	    fi; \
	 done; printf '   -> %s vendored, %s to go\n' "$$have" "$$need"
	@printf '\n-- the payload\n   %s\n' \
	    "$(if $(filter 1,$(SCAFFOLD_ARMED)),armed,not armed: make $(SCAFFOLD_ARM)=1 title once it links)"

ifneq ($(SCAFFOLD_ARMED),1)
.DEFAULT_GOAL := survey

# Unarmed, `make title` still has a packaging rule, and an old `build/<app>.elf` to run it
# over, so it would repackage a payload built before whatever was just fixed and report
# success. Only the two goals that package, and only at the top level: `dist` re-enters make
# with `title` as its goal, and an unarmed title's honest answer to it is "nothing".
ifeq ($(MAKELEVEL),0)
ifneq ($(filter title package,$(MAKECMDGOALS)),)
$(error $(APP_NAME): the payload is behind $(SCAFFOLD_ARM): build it with `make $(SCAFFOLD_ARM)=1 $(firstword $(filter title package,$(MAKECMDGOALS)))`)
endif
endif
endif

endif
