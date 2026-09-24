# RetroArch, through oops-sdk.
#
#   make          fetches upstream at the pinned revision and does nothing else
#
# **A scaffold: the lock, the layout and the notes, with nothing measured yet.** There is no
# source list here because nobody has read upstream's build, and a list written from a glob would
# be a guess wearing the shape of a fact. `docs/PORTING.md` says what is known, what is inherited
# from `../README.md`'s survey, and what the first job is.
#
# **`make` here downloads upstream.** `common/app.mk` includes `common/upstream.mk`, which fetches
# at the revision `upstream.lock` names on the first run and stamps it, so later runs cost
# nothing. `make upstream-clean` removes it.
#
# **No emulation cores.** RetroArch loads them as separate libraries from their own repositories
# and none is pinned here. What this title is queued for is its *renderers* - it ships `gl1.c`,
# `gl2.c` and `gl3.c` as separate drivers, which makes it the one program in the collection that
# exercises all three slots. A core is what makes it useful and is a later question.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# Nothing builds yet, so nothing should fail. See `craft/Makefile` for the shape this grows into:
# a real source list, named and deliberately not armed, beside a `check` that measures the one
# thing deciding whether the title can run at all.
FORMATS = check-only

include $(OOPS_APPS_ROOT)/common/app.mk
