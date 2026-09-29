# The shared POSIX shim for ported titles. Include from a title's Makefile before
# `common/app.mk`:
#
#   include $(OOPS_APPS_ROOT)/common/posix.mk
#
#   EXTRA_TARGET_CFLAGS += $(OOPS_POSIX_INCLUDE) -DOOPS_POSIX_HOME=\"/data/my-title\"
#   PAYLOAD_SRCS        += $(OOPS_POSIX_SRCS)
#
# `stat`, `opendir`, `getpwuid` and their kin are an operating system's interface, not the C
# library, so they live here rather than in oops-sdk's libc. Some answers are not what POSIX
# promises: `chdir` reports whether a directory exists and changes nothing.

ifndef OOPS_POSIX_DIR
OOPS_POSIX_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))/posix
endif

OOPS_POSIX_INCLUDE := -I$(OOPS_POSIX_DIR)/include
OOPS_POSIX_SRCS := $(OOPS_POSIX_DIR)/posix.c

# A hosted title takes its C library from the Mesa sysroot and needs neither `OOPS_POSIX_SRCS`
# nor `OOPS_POSIX_INCLUDE`: the sysroot has those calls, and these headers would shadow it. It
# takes this instead, which holds only what the sysroot declares and no platform library
# exports. Add it to `PAYLOAD_SRCS`; there is no include directory to go with it.
OOPS_POSIX_HOSTED_SRCS := $(OOPS_POSIX_DIR)/hosted.c $(OOPS_POSIX_DIR)/hosted_wchar.c

# `OOPS_POSIX_HOME` is the home directory `getpwuid` reports. A title sets it to its own
# data directory.
