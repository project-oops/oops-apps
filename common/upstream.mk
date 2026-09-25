# **A title whose source is somebody else's** (`src/oops-titles/`), fetched rather than committed.
#
# `common/app.mk` includes this, so a title that does nothing gets the fetch anyway. **A title
# that names files under `upstream/` while its own Makefile is being read must include it
# itself, first:**
#
#   OOPS_APPS_ROOT ?= $(abspath ../../..)
#   include $(OOPS_APPS_ROOT)/common/upstream.mk
#
#   MY_SRCS := $(wildcard upstream/src/*.c)
#
# It is guarded, so the later include from `app.mk` costs one `ifndef`.
#
# # Why a title has to be able to ask for it early
#
# The fetch used to live in `app.mk`, which a title includes **last** - after the lists that say
# what it is made of. Two titles wildcard `upstream/` before that line: Extreme Tux Racer's 45
# C++ sources and Craft's twelve. On a tree that had already been fetched both are correct, and
# on a **fresh checkout both expand to nothing**, because the directory they are looking at does
# not exist yet. The title then builds with its own source missing and the second `make` - after
# the first one's fetch - is right. A build that is wrong once and correct afterwards is the
# worst kind to debug, and it is the same shape as the `make dist` ordering fixed on 2026-09-23.
#
# The opt-in is the presence of `upstream.lock` - no switch to remember, because the file *is*
# the switch, and an app without one is untouched. The lock is `KEY=value` like `app.env`, so
# make reads it with no parser:
#
#     UPSTREAM_KIND=git
#     UPSTREAM_URL=https://github.com/...
#     UPSTREAM_REV=<full commit hash>
#     UPSTREAM_REF=<the tag that hash is, for a reader>
#
# **Why this is a makefile and not a verb on `bin/oops-apps`.** That script says of itself that
# an app is a directory with a Makefile and that it never learns an app's format - it runs
# `make` and takes what appears. A `fetch` verb would put title-specific knowledge in the shared
# CLI and would have to be remembered before `build`; a file that depends on the lock is what
# make is for, so `./bin/oops-apps build <title>` and CI both work with no change to either.
#
# **Why the fetch runs while this file is being read**, rather than from a rule. A title lists
# sources under `upstream/`, and make resolves prerequisites before it runs any recipe - so a
# normal rule would be too late for the very build that needs it. It is deliberately skipped
# for the targets that exist to *remove* things, so `make clean` in a fresh checkout does not
# download 150 MB in order to delete nothing.
#
# **The script is called every time and decides for itself whether to fetch.** It used to be
# called only when the stamp file was *absent*, which sounds like the same thing and is not:
# `upstream-fetch.sh` compares the stamp's *contents* against the revision, the patch set and
# the sparse paths the lock asks for, and that comparison could never run on a tree that had
# already been fetched once. So a lock change on a fetched title did nothing at all - no fetch,
# no message, no error - and the build carried on against the old revision. Widening gl-cts's
# `UPSTREAM_SPARSE` to add `modules/glshared` on 2026-09-24 is what found it; the stamp had to
# be deleted by hand to make the lock take effect, which is not a thing anyone should have to
# know.
#
# That is the same defect the script's own stamp comment describes, one level up, and it
# defeated the fix made there. Calling unconditionally costs one `cksum` over `patches/` and a
# `cat` of the stamp on an up-to-date tree - no network, no `git` - and the script prints
# nothing when there is nothing to do.

ifndef OOPS_UPSTREAM_MK
OOPS_UPSTREAM_MK := 1

OOPS_UPSTREAM_DIR_SELF := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

# Named here as well as in `app.mk`, because this now runs before `app.mk` is read and the
# messages below are the only thing that says which title is downloading 150 MB. It said
# `: fetching upstream at ...` until 2026-09-23, with nothing in front of the colon.
APP_NAME ?= $(notdir $(CURDIR))

ifneq ($(wildcard upstream.lock),)
-include upstream.lock
UPSTREAM_DIR ?= upstream
UPSTREAM_STAMP := $(UPSTREAM_DIR)/.oops-upstream-stamp
ifeq ($(filter clean upstream-clean distclean,$(MAKECMDGOALS)),)
# `UPSTREAM_NAME` and `UPSTREAM_REF` are passed because the script prints the "fetching" line
# now. It has to be the script that prints it: only the script knows whether a fetch is about to
# happen, and the `$(info)` that used to be here was outside that decision.
# **`UPSTREAM_SUBMODULES` is passed for the same reason `UPSTREAM_SPARSE` is.** The script records
# it in the stamp, so a lock that asks for submodules against a script that was never told about
# them produces a stamp mismatch on *every* read of this file - which is a re-fetch on every
# `make`, and on a machine whose container has no `git`, a hard failure on every `make`. Adding a
# key to the script without adding it here is that bug, and it is how this line was found.
$(shell UPSTREAM_NAME="$(APP_NAME)" UPSTREAM_REF="$(UPSTREAM_REF)" \
        UPSTREAM_SPARSE="$(UPSTREAM_SPARSE)" UPSTREAM_SUBMODULES="$(UPSTREAM_SUBMODULES)" \
        $(OOPS_UPSTREAM_DIR_SELF)/upstream-fetch.sh \
        "$(UPSTREAM_KIND)" "$(UPSTREAM_URL)" \
        "$(UPSTREAM_REV)" "$(UPSTREAM_DIR)" "$(CURDIR)/patches" >&2)
ifneq ($(wildcard $(UPSTREAM_STAMP)),$(UPSTREAM_STAMP))
$(error $(APP_NAME): upstream fetch failed - see above)
endif
endif

# `clean` leaves the fetch alone: it is a download, not build output, and re-fetching 150 MB is
# not what anybody means by cleaning a build. This is the target for when they do mean it.
.PHONY: upstream-clean
upstream-clean:
	@rm -rf $(UPSTREAM_DIR)
	@echo "$(APP_NAME): removed $(UPSTREAM_DIR)"
endif

endif
