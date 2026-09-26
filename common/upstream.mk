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

endif
