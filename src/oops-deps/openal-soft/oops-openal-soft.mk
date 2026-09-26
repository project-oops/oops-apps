# OpenAL Soft, over SDL2's audio. Include from a title's Makefile, after `oops-sdl.mk`,
# `oops-libcxx.mk` and `common/posix.mk`, whose include variables it reads:
#
#   OOPS_OPENAL ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/openal-soft)
#   include $(OOPS_OPENAL)/oops-openal-soft.mk
#
#   OOPS_CXX_INCLUDE     += $(OOPS_OPENAL_INCLUDE)
#   EXTRA_TARGET_LDFLAGS += $(OOPS_OPENAL_LDFLAGS)
#   PAYLOAD_EXTRA_DEPS   += $(OOPS_OPENAL_LIBS)
#
# The title must be an exceptions build (`OOPS_CXX_EXCEPTIONS = 1`, libc++abi, libunwind):
# `alc.cpp` catches a backend's failure to open as an exception and tries the next one.
#
# Several sources share a basename (`al/effects/chorus.cpp` and `alc/effects/chorus.cpp`), and
# `ar` names members by basename, so each upstream directory group is its own archive -
# `libopenal-al.a`, `-alc.a` (alc/, alc/effects/), `-backend.a` (alc/backends/) and `-core.a`
# (core/, common/). `oops_ar_check` in `common/deps.mk` refuses a collision.
#
# Linked `--whole-archive` because `app.mk` puts LDFLAGS ahead of the objects that need them
# (see `oops-sdl.mk`); it also makes the order of the four irrelevant.
ifndef OOPS_OPENAL_DIR
OOPS_OPENAL_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif

include $(OOPS_OPENAL_DIR)/../../../common/deps.mk

OOPS_OPENAL_UPSTREAM ?= $(OOPS_OPENAL_DIR)/upstream
OOPS_OPENAL_BUILD    ?= $(OOPS_OPENAL_DIR)/build

# `include/AL` serves `<al.h>` (as SuperTux writes it) and `include` serves `<AL/al.h>`.
# `AL_LIBTYPE_STATIC` is upstream's define for a static link; without it every entry point is
# declared imported.
OOPS_OPENAL_INCLUDE := -I$(OOPS_OPENAL_UPSTREAM)/include -I$(OOPS_OPENAL_UPSTREAM)/include/AL \
                       -DAL_LIBTYPE_STATIC

# Upstream's `COMMON_OBJS`, `CORE_OBJS`, `OPENAL_OBJS` and `ALC_OBJS`, transcribed from
# `CMakeLists.txt` rather than globbed: the directories also hold other platforms' backends
# and the Linux desktop `core/rtkit.cpp` and `core/dbus_wrap.cpp`.
OOPS_OPENAL_AL_SRCS := $(addprefix $(OOPS_OPENAL_UPSTREAM)/al/, \
    auxeffectslot.cpp buffer.cpp effect.cpp error.cpp event.cpp extension.cpp filter.cpp \
    listener.cpp source.cpp state.cpp \
    $(addprefix effects/, autowah.cpp chorus.cpp compressor.cpp convolution.cpp dedicated.cpp \
        distortion.cpp echo.cpp effects.cpp equalizer.cpp fshifter.cpp modulator.cpp null.cpp \
        pshifter.cpp reverb.cpp vmorpher.cpp))

OOPS_OPENAL_ALC_SRCS := $(addprefix $(OOPS_OPENAL_UPSTREAM)/alc/, \
    alc.cpp alu.cpp alconfig.cpp context.cpp device.cpp panning.cpp \
    $(addprefix effects/, autowah.cpp chorus.cpp compressor.cpp convolution.cpp dedicated.cpp \
        distortion.cpp echo.cpp equalizer.cpp fshifter.cpp modulator.cpp null.cpp pshifter.cpp \
        reverb.cpp vmorpher.cpp))

# base, null and loopback are what upstream always builds; `sdl2.cpp` is the one that plays.
OOPS_OPENAL_BACKEND_SRCS := $(addprefix $(OOPS_OPENAL_UPSTREAM)/alc/backends/, \
    base.cpp null.cpp loopback.cpp sdl2.cpp)

# `cpu_caps` chooses between the SSE mixers at run time. Upstream sets the instruction set per
# file; one `-msse4.1` for the whole library is equivalent because the console's CPU has all four.
OOPS_OPENAL_CORE_SRCS := $(addprefix $(OOPS_OPENAL_UPSTREAM)/core/, \
    ambdec.cpp ambidefs.cpp bformatdec.cpp bs2b.cpp bsinc_tables.cpp buffer_storage.cpp \
    context.cpp converter.cpp cpu_caps.cpp cubic_tables.cpp devformat.cpp device.cpp \
    effectslot.cpp except.cpp fmt_traits.cpp fpu_ctrl.cpp helpers.cpp hrtf.cpp logging.cpp \
    mastering.cpp mixer.cpp uhjfilter.cpp uiddefs.cpp voice.cpp \
    filters/biquad.cpp filters/nfc.cpp filters/splitter.cpp \
    mixer/mixer_c.cpp mixer/mixer_sse.cpp mixer/mixer_sse2.cpp mixer/mixer_sse3.cpp \
    mixer/mixer_sse41.cpp) \
  $(addprefix $(OOPS_OPENAL_UPSTREAM)/common/, \
    alcomplex.cpp alfstream.cpp almalloc.cpp alstring.cpp dynload.cpp polyphase_resampler.cpp \
    ringbuffer.cpp strutils.cpp threads.cpp)

# libc++'s wrappers for the C headers come before the C library's (see `common/cxx.mk`).
# `-std=c++14` is upstream's `CXX_STANDARD`.
#
# `RESTRICT`, `AL_BUILD_LIBRARY`, `AL_ALEXT_PROTOTYPES` and the empty `AL_API`/`ALC_API` are what
# upstream's CMake passes for a static build. `ALSOFT_THREAD_LOCAL` is empty, so
# `alcSetThreadContext` is process-wide (`patches/0001`). `-I include` comes first so upstream
# never shadows this directory's `config.h` and `version.h`.
OOPS_OPENAL_CXXFLAGS = -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib \
                       -nostdinc++ -fexceptions -frtti -fPIC -fno-stack-protector \
                       -std=c++14 -O2 -w -msse4.1 \
                       -DRESTRICT=__restrict -DAL_BUILD_LIBRARY -DAL_ALEXT_PROTOTYPES \
                       -DALC_API= -DAL_API= -DALSOFT_THREAD_LOCAL= \
                       -I$(OOPS_OPENAL_DIR)/include \
                       -I$(OOPS_OPENAL_UPSTREAM) -I$(OOPS_OPENAL_UPSTREAM)/common \
                       $(OOPS_OPENAL_INCLUDE) \
                       $(OOPS_SDK_INCLUDE) $(OOPS_LIBCXX_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE) \
                       $(OOPS_POSIX_INCLUDE) $(OOPS_SDL_INCLUDE)

# One archive per upstream directory group.
define oops_openal_archive
OOPS_OPENAL_$(1)_OBJS := $$(call oops_objs,$$(OOPS_OPENAL_BUILD)/obj,$$(OOPS_OPENAL_$(1)_SRCS))
$$(call oops_ar_check,$$(OOPS_OPENAL_$(1)_OBJS))
-include $$(OOPS_OPENAL_$(1)_OBJS:.o=.d)
$$(call oops_obj_rules,$$(OOPS_OPENAL_BUILD)/obj,TARGET_CXX,OOPS_OPENAL_CXXFLAGS,$$(OOPS_OPENAL_$(1)_SRCS))
OOPS_OPENAL_$(1)_LIB := $$(OOPS_OPENAL_BUILD)/libopenal-$(2).a
$$(OOPS_OPENAL_$(1)_LIB): $$(OOPS_OPENAL_$(1)_OBJS)
	@mkdir -p $$(OOPS_OPENAL_BUILD)
	@rm -f $$@
	@ar_tool=$$$$(command -v $$(AR) 2>/dev/null || command -v llvm-ar 2>/dev/null || command -v ar); \
	 "$$$$ar_tool" rcs $$@ $$(OOPS_OPENAL_$(1)_OBJS)
	@echo "openal-soft: $$@ ($$(words $$(OOPS_OPENAL_$(1)_OBJS)) objects)"
endef

$(eval $(call oops_openal_archive,AL,al))
$(eval $(call oops_openal_archive,ALC,alc))
$(eval $(call oops_openal_archive,BACKEND,backend))
$(eval $(call oops_openal_archive,CORE,core))

OOPS_OPENAL_LIBS := $(OOPS_OPENAL_AL_LIB) $(OOPS_OPENAL_ALC_LIB) $(OOPS_OPENAL_BACKEND_LIB) \
                    $(OOPS_OPENAL_CORE_LIB)
OOPS_OPENAL_LDFLAGS := -Wl,--whole-archive $(OOPS_OPENAL_LIBS) -Wl,--no-whole-archive

.PHONY: openal-soft openal-soft-clean
openal-soft: $(OOPS_OPENAL_LIBS)

openal-soft-clean:
	@rm -rf $(OOPS_OPENAL_BUILD)
	@echo "openal-soft: removed build/"
