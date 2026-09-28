# A ported title's upstream source (`src/oops-titles/`), fetched rather than committed.
#
# `common/app.mk` includes this. A title that names files under `upstream/` while its own
# Makefile is read includes it itself, first, so the tree exists on a fresh checkout:
#
#   OOPS_APPS_ROOT ?= $(abspath ../../..)
#   include $(OOPS_APPS_ROOT)/common/upstream.mk
#   MY_SRCS := $(wildcard upstream/src/*.c)
#
# An app opts in by carrying `upstream.lock`, `KEY=value` lines make reads directly:
#
#     UPSTREAM_KIND=git
#     UPSTREAM_URL=https://github.com/...
#     UPSTREAM_REV=<full commit hash>
#     UPSTREAM_REF=<the tag that hash is, for a reader>
#
# The fetch runs while this file is read, because make resolves prerequisites before any
# recipe. `upstream-fetch.sh` runs on every read and compares its stamp against the lock, so
# a lock change takes effect; an up-to-date tree costs no network and prints nothing. The
# removal targets skip it.
#
# A title built from more than one origin - an engine and the game it runs, a program and its
# asset repository - carries one more lock per origin, `upstream-<name>.lock`, with the same
# keys. It is fetched into `upstream-<name>/`, with its patches in `patches/<name>/`, and a
# failed fetch stops the build the same way. Its keys are read by the shell rather than by
# make, so they do not overwrite the primary lock's `UPSTREAM_*`.

ifndef OOPS_UPSTREAM_MK
OOPS_UPSTREAM_MK := 1

OOPS_UPSTREAM_DIR_SELF := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

# Set here too, since this can be read before `app.mk`, and the fetch messages name the app.
APP_NAME ?= $(notdir $(CURDIR))

ifneq ($(wildcard upstream.lock),)
-include upstream.lock
UPSTREAM_DIR ?= upstream
UPSTREAM_STAMP := $(UPSTREAM_DIR)/.oops-upstream-stamp
ifeq ($(filter clean upstream-clean distclean,$(MAKECMDGOALS)),)
# Every lock key the script records in its stamp is passed, or the stamp never matches and
# every `make` re-fetches. The script prints the "fetching" line, since only it knows.
$(shell UPSTREAM_NAME="$(APP_NAME)" UPSTREAM_REF="$(UPSTREAM_REF)" \
        UPSTREAM_SPARSE="$(UPSTREAM_SPARSE)" UPSTREAM_SUBMODULES="$(UPSTREAM_SUBMODULES)" \
        $(OOPS_UPSTREAM_DIR_SELF)/upstream-fetch.sh \
        "$(UPSTREAM_KIND)" "$(UPSTREAM_URL)" \
        "$(UPSTREAM_REV)" "$(UPSTREAM_DIR)" "$(CURDIR)/patches" >&2)
ifneq ($(wildcard $(UPSTREAM_STAMP)),$(UPSTREAM_STAMP))
$(error $(APP_NAME): upstream fetch failed - see above)
endif
endif

# `clean` keeps the fetched tree, which is a download rather than build output; this removes it.
.PHONY: upstream-clean
upstream-clean:
	@rm -rf $(UPSTREAM_DIR)
	@echo "$(APP_NAME): removed $(UPSTREAM_DIR)"
endif

# The further origins. `set -a` exports what the lock sets, which is how the script reads
# `UPSTREAM_SPARSE` and `UPSTREAM_SUBMODULES`; the subshell keeps them from leaking.
UPSTREAM_EXTRA := $(patsubst upstream-%.lock,%,$(wildcard upstream-*.lock))
ifneq ($(UPSTREAM_EXTRA),)
ifeq ($(filter clean upstream-clean distclean,$(MAKECMDGOALS)),)
$(foreach n,$(UPSTREAM_EXTRA),$(shell ( set -a; . ./upstream-$(n).lock; set +a; \
    UPSTREAM_NAME="$(APP_NAME)/$(n)" $(OOPS_UPSTREAM_DIR_SELF)/upstream-fetch.sh \
    "$$UPSTREAM_KIND" "$$UPSTREAM_URL" "$$UPSTREAM_REV" "upstream-$(n)" \
    "$(CURDIR)/patches/$(n)" ) >&2))
$(foreach n,$(UPSTREAM_EXTRA),$(if $(wildcard upstream-$(n)/.oops-upstream-stamp),,\
    $(error $(APP_NAME): fetch of upstream-$(n) failed - see above)))
endif

upstream-clean: upstream-extra-clean
.PHONY: upstream-extra-clean
upstream-extra-clean:
	@rm -rf $(addprefix upstream-,$(UPSTREAM_EXTRA))
	@echo "$(APP_NAME): removed $(addprefix upstream-,$(UPSTREAM_EXTRA))"
endif

endif
