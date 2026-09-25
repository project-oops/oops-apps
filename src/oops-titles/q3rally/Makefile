# Q3Rally, through oops-sdk.
#
#   make          fetches upstream at the pinned revision and does nothing else
#
# **A scaffold: the lock, the layout and one measurement.** There is no source list here because
# nobody has read upstream's build yet, and a list written from a glob would be a guess wearing the
# shape of a fact. What *has* been measured is the GL surface - see docs/PORTING.md, which gives
# the number and how it was taken.
#
# **`make` here downloads upstream.** `common/app.mk` includes `common/upstream.mk`, which fetches
# at the revision `upstream.lock` names on the first run and stamps it, so later runs cost nothing.
# `make upstream-clean` removes it.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# Nothing builds yet, so nothing should fail. `craft/Makefile` is the shape this grows into: a real
# source list, named and deliberately not armed, beside a `check` that measures the one thing
# deciding whether the title can run at all.
FORMATS = check-only

include $(OOPS_APPS_ROOT)/common/app.mk
