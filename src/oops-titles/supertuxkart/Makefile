# SuperTuxKart, through oops-sdk.
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
# **`stk-assets` is not fetched and is about a gigabyte.** This lock covers `stk-code`; the game
# does not start without the other repository, and which mechanism brings it is undecided -
# `upstream.lock` and `docs/PORTING.md` both say so. Nothing here will surprise you with it.

OOPS_APPS_ROOT ?= $(abspath ../../..)

# Nothing builds yet, so nothing should fail. See `craft/Makefile` for the shape this grows into:
# a real source list, named and deliberately not armed, beside a `check` that measures the one
# thing deciding whether the title can run at all.
FORMATS = check-only

include $(OOPS_APPS_ROOT)/common/app.mk
