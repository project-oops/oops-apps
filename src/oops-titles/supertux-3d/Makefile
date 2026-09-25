# SuperTux 3D, through oops-sdk.
#
#   make          fetches upstream at the pinned revision and does nothing else
#
# **This title has no build and the reason is not a missing measurement.** It is a Godot project -
# 22 `.gd` scripts and their scenes, no engine source - so there is nothing here to compile against
# oops-sdk. What would have to exist first is Godot itself on this target, which is an engine job
# rather than a title one. docs/PORTING.md sets out what that involves.
#
# The scaffold exists so the decision is written down somewhere a reader will find it, next to the
# other candidates from the same survey, rather than living in a chat log.

OOPS_APPS_ROOT ?= $(abspath ../../..)

FORMATS = check-only

include $(OOPS_APPS_ROOT)/common/app.mk
