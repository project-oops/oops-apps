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
# **The title must be an exceptions build** (`OOPS_CXX_EXCEPTIONS = 1`, libc++abi, libunwind).
# OpenAL Soft reports a backend that will not open by throwing, and `alc.cpp` catches it to try
# the next one - so this is compiled `-fexceptions -frtti`, and a title without the unwinder
# would reach `std::terminate` the first time a device failed to open.
#
# # Four archives, because `ar` names members by basename
#
# Sixteen of the ninety sources share a file name with another: `al/effects/chorus.cpp` is the
# API object and `alc/effects/chorus.cpp` the DSP, `alc/context.cpp` and `core/context.cpp` are
# different layers, and `null.cpp` exists three times. One archive would keep one of each pair
# and drop the other without a word - `common/deps.mk`'s `oops_ar_check` refuses exactly that.
# Upstream's own directories are the split that has no collisions, so each is its own archive:
#
#   libopenal-al.a        al/                 the AL API: sources, buffers, effects objects
#   libopenal-alc.a       alc/, alc/effects/  the ALC API, the mixer driver, the effect DSP
#   libopenal-backend.a   alc/backends/       base, null, loopback, SDL2
#   libopenal-core.a      core/, common/      the mixer, filters, resampler, threads
#
# Linked `--whole-archive`, for the reason `oops-sdl.mk` gives: `app.mk` puts LDFLAGS ahead of
# the objects that need them. It also removes any question of order between the four.
ifndef OOPS_OPENAL_DIR
OOPS_OPENAL_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif

include $(OOPS_OPENAL_DIR)/../../../common/deps.mk

OOPS_OPENAL_UPSTREAM ?= $(OOPS_OPENAL_DIR)/upstream
OOPS_OPENAL_BUILD    ?= $(OOPS_OPENAL_DIR)/build

# `include/AL` is what a program asks for as `<al.h>` - SuperTux does - and `include` is `<AL/al.h>`.
# `AL_LIBTYPE_STATIC` is upstream's public define for a static link: without it the headers
# declare every entry point as imported.
OOPS_OPENAL_INCLUDE := -I$(OOPS_OPENAL_UPSTREAM)/include -I$(OOPS_OPENAL_UPSTREAM)/include/AL \
                       -DAL_LIBTYPE_STATIC

# --- the sources: upstream's `COMMON_OBJS`, `CORE_OBJS`, `OPENAL_OBJS`, `ALC_OBJS` -----------
#
# Transcribed from `CMakeLists.txt` at the pin rather than globbed, because the directories also
# hold what a build must *not* take: fifteen output backends for other platforms under
# `alc/backends/`, and `core/rtkit.cpp` and `core/dbus_wrap.cpp`, which are Linux desktop
# session plumbing.
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

# The four SSE mixers are compiled with the instruction set each is named for, and `cpu_caps`
# chooses between them at run time - the arrangement upstream's CMake makes with per-file flags.
# One flag for the whole library does the same thing more simply here, because the console's
# CPU has all four: nothing compiled for SSE 4.1 can reach a CPU without it.
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

# --- flags ------------------------------------------------------------------
#
# The libc++ ordering is `common/cxx.mk`'s and so is the reason: libc++'s wrappers for the C
# headers must be found before the C library's. `-std=c++14` is upstream's `CXX_STANDARD`.
#
# `RESTRICT`, `AL_BUILD_LIBRARY`, `AL_ALEXT_PROTOTYPES` and the empty `AL_API`/`ALC_API` are what
# upstream's CMake passes for a static build. `ALSOFT_THREAD_LOCAL` empty is ours, and
# `patches/0001` says what it costs: no thread-local storage, whose support on this target is
# unmeasured, at the price of `alcSetThreadContext` being process-wide. `-I include` for our `config.h` and `version.h`,
# ahead of everything else so nothing of upstream's shadows them.
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

# --- the four archives -----------------------------------------------------
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
