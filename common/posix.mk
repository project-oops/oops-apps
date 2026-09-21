# The shared POSIX shim for ported titles. Include from a title's Makefile before
# `common/app.mk`:
#
#   include $(OOPS_APPS_ROOT)/common/posix.mk
#
#   EXTRA_TARGET_CFLAGS += $(OOPS_POSIX_INCLUDE) -DOOPS_POSIX_HOME=\"/data/my-title\"
#   PAYLOAD_SRCS        += $(OOPS_POSIX_SRCS)
#
# # What this is, and why it is not in oops-sdk
#
# `stat`, `opendir`, `getpwuid` and their kin are an operating system's interface, not the C
# standard library. `oops-sdk`'s libc is what any payload can expect; this is what a *port* needs
# because it was written for a desktop, and some of the answers here are deliberately not what
# POSIX promises - `chdir` reports whether a directory exists and changes nothing. That kind of
# compromise belongs where a reader is looking for it.
#
# # It is shared because two titles wanted it
#
# It was Extreme Tux Racer's private shim first. Neverball then needed the same thing, and the
# measurement made the case on its own: Neverball compiled **8 of its 86 sources** without this
# on the include path and **57** with it. One header - `sys/stat.h`, reached by 65 files through
# a single `share/dir.h` - was most of that.
#
# The same pattern sent `errno` one level further down, into `oops-sdk`, after a third consumer.

ifndef OOPS_POSIX_DIR
OOPS_POSIX_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))/posix
endif

OOPS_POSIX_INCLUDE := -I$(OOPS_POSIX_DIR)/include
OOPS_POSIX_SRCS := $(OOPS_POSIX_DIR)/posix.c

# `OOPS_POSIX_HOME` is where `getpwuid` points a program that asks for a home directory. A title
# sets it; the default names no title and is a poor place to write, which is the intended nudge.
