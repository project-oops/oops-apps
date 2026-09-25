# OOPSy-DAISY - install oops-apps homebrew onto the console.
#
#   make check      host self-test of the pure parts (catalogue parse, queue, JSON, the page)
#   make skeleton   the payload compiles freestanding for the target, object only
#   make elf        freestanding payload ELF for the target console
#
# The core runtime (libc, heap, fs, sysmodule, time, math) is added by common/app.mk; this lists
# the subsystems the app drives with OOPS_FEATURES. `http` brings the HTTPS client *and* the zip
# extractor (the SDK groups zip.c under it), so the installer half links either way; there is no
# separate `zip` feature to add, and listing it would double zip.c.
#
# # The on-device UI is the oops-apps index page, rendered by the SDK webview
#
# oopsy-daisy_main.c drives the SDK webview (oops/webview.h) to render oopsy-daisy_page.c - the
# oops-apps catalogue page, litehtml-friendly (flex, no CSS grid) and bridged to the native install
# queue. That path is compiled in only when -DOOPSY_USE_WEBVIEW is set; without it the app builds
# the native canvas menu, which needs only the C features listed here and always links.
#
# **The webview is not wired on by default, on purpose.** It is C++ (litehtml + the DOM bridge)
# over a freestanding libc++, and the html feature also carries gumbo, third-party C that the
# strict payload C flags (-Werror -Wconversion) reject. Turning it on therefore needs the build to
# learn to compile the `html`/`webview` features as C++/relaxed-C and to link
# libc++/libc++abi/libunwind with the eh-frame placement cxx-throw pins down (oops-apps#D005) -
# an app.mk/SDK change, tracked separately. Once that lands the switch is a one-liner:
#
#   EXTRA_TARGET_CFLAGS += -DOOPSY_USE_WEBVIEW
#   OOPS_FEATURES       += js html webview      # once app.mk compiles C++ features
#   ...libc++/libc++abi/libunwind + cxx.mk wiring, as in cxx-throw's Makefile...
#
# The self-test proves the page and the bridge JSON regardless, so the UI logic is under test now.

OOPS_APPS_ROOT ?= $(abspath ../../..)
OOPS_SDK       ?= $(abspath $(OOPS_APPS_ROOT)/../oops-sdk)

OOPS_FEATURES := display draw input keyboard net http

HOST_TEST_SRCS = oopsy-daisy_selftest.c oopsy-daisy.c oopsy-daisy_page.c $(OOPS_FEATURE_draw_SRCS)

PAYLOAD_SRCS = oopsy-daisy.c oopsy-daisy_main.c oopsy-daisy_page.c

ENTRY_POINT := oopsy_daisy_start

# ---------------------------------------------------------------------------
# Webview UI (opt-in): build the SDK web stack into the payload.
#
#   make OOPSY_WEBVIEW=1 elf
#
# The `html`/`webview` features are C++ (litehtml + the DOM bridge) and `js` is QuickJS; litehtml
# also carries gumbo. None of that survives the strict payload C flags, so this does the split the
# OOPS_FEATURES consumer cannot yet do:
#   - litehtml + our html/webview glue (C++) -> the cxx.mk archive, at -std=c++14 (litehtml needs
#     deduced return types) with exceptions on (litehtml throws), against the pinned freestanding
#     libc++/libc++abi/libunwind (prebuilt under oops-deps/libcxx).
#   - QuickJS + gumbo (third-party C) -> a relaxed-C archive (-w, no -Werror), since they will not
#     pass -Werror -Wconversion.
# -DOOPSY_USE_WEBVIEW then compiles oopsy-daisy_main.c's webview path in. Everything is
# --whole-archive because app.mk puts LDFLAGS before the payload objects on the link line.
ifeq ($(OOPSY_WEBVIEW),1)
OOPS_LIBCXX ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/libcxx)

# The freestanding libc++ is built with external threading (_LIBCPP_HAS_THREAD_API_EXTERNAL), so its
# std::mutex/condition_variable route to the SDK's oops_mutex_*/oops_cond_*/oops_thread_* - which the
# `thread` feature provides. libc++ needs it the moment any threaded TU (shared_ptr's atomics, etc.)
# is pulled in.
OOPS_FEATURES += thread

# A separate build tree from the native default, so toggling OOPSY_WEBVIEW does not read objects
# compiled under the other setting (the flag is not a make prerequisite, so a shared tree would
# silently reuse stale objects). The C++/relaxed-C sub-archives live under it too.
BUILD          := build-webview
OOPS_CXX_BUILD := $(BUILD)/cxx

OOPS_CXX_STD        := c++20
OOPS_CXX_EXCEPTIONS := 1
OOPS_CXX_SRCS := \
    $(CURDIR)/webview-cxx-glue.cpp \
    $(OOPS_SDK)/src/html/oops_html.cpp \
    $(OOPS_SDK)/src/html/html_container.cpp \
    $(wildcard $(OOPS_SDK)/src/html/litehtml/src/*.cpp) \
    $(OOPS_SDK)/src/webview/webview.cpp \
    $(OOPS_SDK)/src/webview/dom_bridge.cpp \
    $(OOPS_SDK)/src/webview/host_env.cpp

include $(OOPS_LIBCXX)/oops-libcxx.mk
include $(OOPS_LIBCXX)/oops-libunwind.mk
include $(OOPS_LIBCXX)/oops-libcxxabi.mk
include $(OOPS_APPS_ROOT)/common/cxx.mk

# litehtml's headers (both `litehtml/x.h` and the bare `x.h` its own sources use, which live in
# include/litehtml), gumbo, QuickJS (for quickjs/quickjs.h), and libc++ reach the C++ compile.
# libc++ before the SDK libc is cxx.mk's job (OOPS_CXX_INCLUDE precedes OOPS_SDK_LIBC_INCLUDE in
# its flag order).
# NB: -I .../js/quickjs is deliberately NOT here - that dir holds a file literally named `version`,
# which would shadow libc++'s C++17 <version> header. webview.cpp reaches QuickJS as
# `quickjs/quickjs.h` via -I .../src/js instead. The shim closes a gap in the SDK's freestanding
# libc that only surfaces because this is the first freestanding title to link libc++ (ldiv/lldiv);
# it is force-included ahead of libc++'s <stdlib.h>.
# litehtml's string interning guards its std::mutex behind LITEHTML_NO_THREADS; the SDK's
# freestanding libc++ has every thread API off (single-threaded payload), so build it that way.
OOPS_CXX_INCLUDE += \
    -DLITEHTML_NO_THREADS \
    -I$(OOPS_SDK)/src/html \
    -I$(OOPS_SDK)/src/webview \
    -I$(OOPS_SDK)/src/html/litehtml/include \
    -I$(OOPS_SDK)/src/html/litehtml/include/litehtml \
    -I$(OOPS_SDK)/src/html/litehtml/src \
    -I$(OOPS_SDK)/src/html/litehtml/src/gumbo/include \
    -I$(OOPS_SDK)/src/js \
    -include $(CURDIR)/webview-libcxx-shim.h \
    $(OOPS_LIBCXX_INCLUDE)

# The relaxed-C side: QuickJS and gumbo, compiled as C with warnings off.
OOPSY_WEBC_SRCS := \
    $(CURDIR)/webview-libc-extra.c \
    $(OOPS_SDK)/src/js/js.c \
    $(wildcard $(OOPS_SDK)/src/js/quickjs/*.c) \
    $(wildcard $(OOPS_SDK)/src/html/litehtml/src/gumbo/*.c)
OOPSY_WEBC_BUILD  := $(BUILD)/webc
OOPSY_WEBC_LIB    := $(OOPSY_WEBC_BUILD)/libwebc.a
# -Dtm_gmtoff=tm_isdst: QuickJS reads tm.tm_gmtoff once, for getTimezoneOffset. The SDK's struct tm
# has no timezone fields (it is a UTC-only calendar), so the offset is 0; remapping the access to the
# existing, always-zero tm_isdst compiles it and yields the correct UTC offset without editing the
# SDK header.
OOPSY_WEBC_CFLAGS  = -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib -fPIC \
                     -fno-stack-protector -fvisibility=hidden -std=c11 -O2 -w -nostdlibinc \
                     -Dtm_gmtoff=tm_isdst \
                     -I$(CURDIR)/webview-shim \
                     -I$(OOPS_SDK)/include -I$(OOPS_SDK) -I$(OOPS_SDK)/include/libc \
                     -I$(OOPS_SDK)/src/js \
                     -I$(OOPS_SDK)/src/html/litehtml/src \
                     -I$(OOPS_SDK)/src/html/litehtml/src/gumbo/include \
                     -I$(OOPS_SDK)/src/html/litehtml/src/gumbo/include/gumbo
OOPSY_WEBC_OBJS   := $(call oops_objs,$(OOPSY_WEBC_BUILD)/obj,$(OOPSY_WEBC_SRCS))
-include $(OOPSY_WEBC_OBJS:.o=.d)
$(call oops_obj_rules,$(OOPSY_WEBC_BUILD)/obj,TARGET_CC,OOPSY_WEBC_CFLAGS,$(OOPSY_WEBC_SRCS))
$(OOPSY_WEBC_LIB): $(OOPSY_WEBC_OBJS)
	@mkdir -p $(OOPSY_WEBC_BUILD)
	@rm -f $@
	@ar_tool=$$(command -v $(AR) 2>/dev/null || command -v llvm-ar 2>/dev/null || command -v ar); \
	 "$$ar_tool" rcs $@ $(OOPSY_WEBC_OBJS)
	@echo "webc: $@ ($(words $(OOPSY_WEBC_OBJS)) objects)"
OOPSY_WEBC_LDFLAGS := -Wl,--whole-archive $(OOPSY_WEBC_LIB) -Wl,--no-whole-archive

# The prebuilt libc++ archive was built without its memory/functional/variant translation units, so
# it is missing the typeinfos for std::bad_weak_ptr / bad_function_call / bad_variant_access and the
# internal std::__shared_weak_count. mkmodule refuses a module with an imported symbol it cannot map
# to a library, so these must be *defined*, not allow-listed. Compile exactly those three upstream
# libc++ sources into their own archive, with -D_LIBCPP_BUILDING_LIBRARY scoped to them (defining it
# globally would change libc++'s headers under litehtml). Duplicates with libc++.a are taken by the
# --allow-multiple-definition already on the link. Proper fix is a complete libc++ build, SDK-side
# (REQ-20260925T0110Z-e1d9).
OOPSY_LXSRC_SRCS := \
    $(OOPS_LIBCXX)/upstream/libcxx/src/memory.cpp \
    $(OOPS_LIBCXX)/upstream/libcxx/src/functional.cpp \
    $(OOPS_LIBCXX)/upstream/libcxx/src/variant.cpp
OOPSY_LXSRC_BUILD  := $(BUILD)/lxsrc
OOPSY_LXSRC_LIB    := $(OOPSY_LXSRC_BUILD)/liblxsrc.a
OOPSY_LXSRC_FLAGS   = $(OOPS_CXX_FLAGS) -D_LIBCPP_BUILDING_LIBRARY
OOPSY_LXSRC_OBJS   := $(call oops_objs,$(OOPSY_LXSRC_BUILD)/obj,$(OOPSY_LXSRC_SRCS))
-include $(OOPSY_LXSRC_OBJS:.o=.d)
$(call oops_obj_rules,$(OOPSY_LXSRC_BUILD)/obj,TARGET_CXX,OOPSY_LXSRC_FLAGS,$(OOPSY_LXSRC_SRCS))
$(OOPSY_LXSRC_LIB): $(OOPSY_LXSRC_OBJS)
	@mkdir -p $(OOPSY_LXSRC_BUILD)
	@rm -f $@
	@ar_tool=$$(command -v $(AR) 2>/dev/null || command -v llvm-ar 2>/dev/null || command -v ar); \
	 "$$ar_tool" rcs $@ $(OOPSY_LXSRC_OBJS)
	@echo "lxsrc: $@ ($(words $(OOPSY_LXSRC_OBJS)) objects)"
OOPSY_LXSRC_LDFLAGS := -Wl,--whole-archive $(OOPSY_LXSRC_LIB) -Wl,--no-whole-archive

# Linking full libc++ means libc++ owns operator new/delete (in its __lcxx_override section, with a
# guard that every replaceable form resolve inside it); cxxrt.cpp must stand aside or its new wins
# outside that section and libc++'s nothrow new traps. Allocation still lands on the SDK heap
# (oops-sdk malloc/free ARE oops_malloc/oops_free).
EXTRA_TARGET_CFLAGS  += -DOOPSY_USE_WEBVIEW -DOOPS_CXX_EXTERNAL_NEW_DELETE
# The prebuilt freestanding libc++ and libc++abi archives both define the std:: exception ABI
# classes (std::exception, std::bad_alloc, std::terminate, ...): a normal llvm build has libc++abi
# own them and libc++ defer, but no freestanding title had linked both together before, so the
# overlap never showed. libc++abi comes first in the link order below (it is the ABI owner), so
# --allow-multiple-definition takes its copy; the two are the same ABI-stable classes. Filed for the
# SDK to build the pair without the overlap (REQ-20260925T0110Z-e1d9).
EXTRA_TARGET_LDFLAGS += -Wl,--allow-multiple-definition \
                        $(OOPS_CXX_LDFLAGS) $(OOPSY_WEBC_LDFLAGS) $(OOPSY_LXSRC_LDFLAGS) \
                        $(OOPS_LIBCXXABI_LDFLAGS) $(OOPS_LIBCXX_LDFLAGS) $(OOPS_LIBUNWIND_LDFLAGS)
PAYLOAD_EXTRA_DEPS   += $(OOPS_CXX_LIB) $(OOPSY_WEBC_LIB) $(OOPSY_LXSRC_LIB) \
                        $(OOPS_LIBCXXABI_LIB) $(OOPS_LIBCXX_LIB) $(OOPS_LIBUNWIND_LIB)
endif

include $(OOPS_APPS_ROOT)/common/app.mk
