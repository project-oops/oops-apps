# StormLib build integration.
#
#   OOPS_STORMLIB ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/stormlib)
#   include $(OOPS_STORMLIB)/oops-stormlib.mk
#   EXTRA_TARGET_CFLAGS += $(OOPS_STORMLIB_INCLUDE)
#   EXTRA_LDFLAGS       += $(OOPS_STORMLIB_LDFLAGS)
#
# MPQ archive reading. Ship of Harkinian keeps a title's extracted assets in one, so this is the
# dependency between "the port builds" and "the port can open anything".
#
# It needs zlib, so include `oops-zlib.mk` first - see `OOPS_STORMLIB_CFLAGS` below for why that
# is a hard requirement here rather than the bundled copy upstream would otherwise use.
ifndef OOPS_STORMLIB_DIR
OOPS_STORMLIB_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
OOPS_STORMLIB_UPSTREAM ?= $(OOPS_STORMLIB_DIR)/upstream
OOPS_STORMLIB_BUILD ?= $(OOPS_STORMLIB_DIR)/build

OOPS_STORMLIB_INCLUDE := -I$(OOPS_STORMLIB_UPSTREAM)/src
OOPS_STORMLIB_LIB := $(OOPS_STORMLIB_BUILD)/libstorm.a
OOPS_STORMLIB_LDFLAGS := $(OOPS_STORMLIB_LIB)

# **`SRC_FILES` is not the whole library, and reading only it cost a link.** Upstream assembles the
# target from four lists - `SRC_FILES`, `TOMCRYPT_FILES`, `TOMMATH_FILES` and, when the platform
# has no system bzip2, `BZIP2_FILES` - and only the first carries that name. An archive built from
# `SRC_FILES` alone compiles cleanly and is missing 201 sources: `SFileVerify.cpp` is in it and
# calls `rsa_verify_hash_ex`, `SCompression.cpp` calls `BZ2_bzDecompress`, and neither is defined.
# A payload link does not report that, so the first sign would have been a jump into nothing on
# the console. `nm --undefined-only` on the archive is what found it, and is worth running after
# any upstream bump here.
#
# The three extra lists are whole directories, so they are globbed - and the glob is checked. Each
# one's count is asserted below against what upstream's `CMakeLists.txt` names, because a glob's
# failure mode is silently building a different library than upstream does.
#
# **The split into C and C++ is load-bearing, not tidiness.** These are separate variables because
# the two halves compile with different front ends, and a probe that ran the whole list through
# `clang++` reported `libtomcrypt/.../rsa_verify_simple.c` as broken for a day. It is not: it is C,
# where `void *` converts to `unsigned char *` without a cast, and C++ is the language that
# refuses it. Compiling a `.c` as C++ here does not merely warn, it invents a defect in upstream.
OOPS_STORMLIB_CXX_SRCS := \
    $(OOPS_STORMLIB_UPSTREAM)/src/adpcm/adpcm.cpp \
    $(OOPS_STORMLIB_UPSTREAM)/src/huffman/huff.cpp \
    $(OOPS_STORMLIB_UPSTREAM)/src/sparse/sparse.cpp \
    $(OOPS_STORMLIB_UPSTREAM)/src/FileStream.cpp \
    $(OOPS_STORMLIB_UPSTREAM)/src/SBaseCommon.cpp \
    $(OOPS_STORMLIB_UPSTREAM)/src/SBaseDumpData.cpp \
    $(OOPS_STORMLIB_UPSTREAM)/src/SBaseFileTable.cpp \
    $(OOPS_STORMLIB_UPSTREAM)/src/SBaseSubTypes.cpp \
    $(OOPS_STORMLIB_UPSTREAM)/src/SCompression.cpp \
    $(OOPS_STORMLIB_UPSTREAM)/src/SFileAddFile.cpp \
    $(OOPS_STORMLIB_UPSTREAM)/src/SFileAttributes.cpp \
    $(OOPS_STORMLIB_UPSTREAM)/src/SFileCompactArchive.cpp \
    $(OOPS_STORMLIB_UPSTREAM)/src/SFileCreateArchive.cpp \
    $(OOPS_STORMLIB_UPSTREAM)/src/SFileExtractFile.cpp \
    $(OOPS_STORMLIB_UPSTREAM)/src/SFileFindFile.cpp \
    $(OOPS_STORMLIB_UPSTREAM)/src/SFileGetFileInfo.cpp \
    $(OOPS_STORMLIB_UPSTREAM)/src/SFileListFile.cpp \
    $(OOPS_STORMLIB_UPSTREAM)/src/SFileOpenArchive.cpp \
    $(OOPS_STORMLIB_UPSTREAM)/src/SFileOpenFileEx.cpp \
    $(OOPS_STORMLIB_UPSTREAM)/src/SFilePatchArchives.cpp \
    $(OOPS_STORMLIB_UPSTREAM)/src/SFileReadFile.cpp \
    $(OOPS_STORMLIB_UPSTREAM)/src/SFileVerify.cpp
OOPS_STORMLIB_C_SRCS := \
    $(OOPS_STORMLIB_UPSTREAM)/src/jenkins/lookup3.c \
    $(OOPS_STORMLIB_UPSTREAM)/src/lzma/C/LzFind.c \
    $(OOPS_STORMLIB_UPSTREAM)/src/lzma/C/LzmaDec.c \
    $(OOPS_STORMLIB_UPSTREAM)/src/lzma/C/LzmaEnc.c \
    $(OOPS_STORMLIB_UPSTREAM)/src/pklib/explode.c \
    $(OOPS_STORMLIB_UPSTREAM)/src/pklib/implode.c

# The crypto pair, for `SFileVerify`. `TOMCRYPT_FILES` names 76 files and `SRC_FILES` names two
# more from the same tree, which together are every `.c` under it - so the glob is the union, not
# a superset.
OOPS_STORMLIB_TOMCRYPT_SRCS := $(shell find $(OOPS_STORMLIB_UPSTREAM)/src/libtomcrypt -name '*.c')
OOPS_STORMLIB_TOMMATH_SRCS  := $(shell find $(OOPS_STORMLIB_UPSTREAM)/src/libtommath -name '*.c')
# Bundled bzip2, because there is no system one to find. MPQ compression method 0x10 is bzip2 and
# `SCompression.cpp` dispatches to it from a table, so it is reachable from any archive whose
# packer chose it - not something a reader gets to opt out of.
OOPS_STORMLIB_BZIP2_SRCS    := $(shell find $(OOPS_STORMLIB_UPSTREAM)/src/bzip2 -name '*.c')
OOPS_STORMLIB_C_SRCS += $(OOPS_STORMLIB_TOMCRYPT_SRCS) $(OOPS_STORMLIB_TOMMATH_SRCS) \
                        $(OOPS_STORMLIB_BZIP2_SRCS)

# The guard on the globs above. These are upstream's counts at the pinned revision; a bump that
# adds or drops a file fails here with the name of the set rather than at a link months later.
OOPS_STORMLIB_EXPECT := 78:$(words $(OOPS_STORMLIB_TOMCRYPT_SRCS)):libtomcrypt \
                        118:$(words $(OOPS_STORMLIB_TOMMATH_SRCS)):libtommath \
                        7:$(words $(OOPS_STORMLIB_BZIP2_SRCS)):bzip2
$(foreach e,$(OOPS_STORMLIB_EXPECT),\
  $(if $(filter $(word 1,$(subst :, ,$(e))),$(word 2,$(subst :, ,$(e)))),,\
    $(error StormLib: $(word 3,$(subst :, ,$(e))) has $(word 2,$(subst :, ,$(e))) sources, \
            upstream's CMakeLists names $(word 1,$(subst :, ,$(e))) - re-check oops-stormlib.mk \
            against CMakeLists.txt after this bump)))

# `__SYS_ZLIB` is upstream's switch for "the platform has zlib, do not build the bundled copy".
# Taking it is not about saving a build: StormLib's bundled zlib and the one under
# `oops-deps/zlib` would both define `inflate`, and a payload link resolves a duplicate symbol
# without complaint - so the version that won would be whichever the linker reached first.
#
# `_7ZIP_ST` is LZMA's single-threaded build. The alternative wants `CreateThread` and a Windows
# event object; the ports here decompress on the calling thread.
#
# `__PROSPERO__` is what `patches/0001` keys on. Upstream's `StormPort.h:309` reaches for
# `sys/mman.h` on every BSD, and this target is a FreeBSD without one; the patch narrows that arm
# rather than adding a platform, so the define has to be on the command line for every source -
# each dependency here sets it for itself, the same way the SDL family does.
#
# `BZ_STRICT_ANSI` is upstream's own `add_definitions`, and it earns its place here: it stops
# bzip2 reaching for `<sys/stat.h>` open-mode machinery it only needs for its command-line tool.
OOPS_STORMLIB_DEFS := -D__SYS_ZLIB -D_7ZIP_ST -DBZ_STRICT_ANSI -D__PROSPERO__=1
OOPS_STORMLIB_TARGET = -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib \
                       -fPIC -O2 -w
OOPS_STORMLIB_OWN    = $(OOPS_STORMLIB_INCLUDE) $(OOPS_STORMLIB_DEFS) $(OOPS_ZLIB_INCLUDE)
OOPS_STORMLIB_CFLAGS = $(OOPS_STORMLIB_TARGET) -nostdlibinc -std=gnu11 $(OOPS_STORMLIB_OWN) \
                       $(OOPS_POSIX_INCLUDE) $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE)
# **`$(OOPS_LIBCXX_INCLUDE)` comes before the C headers.** libc++ ships its own `<math.h>` and
# `<stdlib.h>` wrapping the C library's, and `<cmath>` stops with an explicit error if it reaches
# the C one first. StormLib's C++ is C-with-classes and includes neither, so the wrong order built
# perfectly well here and would have failed for the next consumer - prism-processor is where it
# actually showed up. It also carries `-nostdinc++ -nostdlibinc`, so those are not repeated.
OOPS_STORMLIB_CXXFLAGS = $(OOPS_STORMLIB_TARGET) -std=c++17 $(OOPS_STORMLIB_OWN) \
                         $(OOPS_LIBCXX_INCLUDE) \
                         $(OOPS_POSIX_INCLUDE) $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE)

# Objects are named by a counter rather than by basename: `src/lzma/C/LzFind.c` and a future
# `src/LzFind.c` would collide in one flat build directory, and a silently-overwritten object is
# the kind of failure that shows up as a missing symbol three links later.
$(OOPS_STORMLIB_LIB): $(OOPS_STORMLIB_C_SRCS) $(OOPS_STORMLIB_CXX_SRCS) $(lastword $(MAKEFILE_LIST))
	@mkdir -p $(OOPS_STORMLIB_BUILD)
	@rm -f $@
	@n=0; objs=""; \
	 for s in $(OOPS_STORMLIB_C_SRCS); do n=$$((n+1)); o=$(OOPS_STORMLIB_BUILD)/s$$n.o; \
	   $(TARGET_CC) $(OOPS_STORMLIB_CFLAGS) -c -o "$$o" "$$s" || exit 1; objs="$$objs $$o"; done; \
	 for s in $(OOPS_STORMLIB_CXX_SRCS); do n=$$((n+1)); o=$(OOPS_STORMLIB_BUILD)/s$$n.o; \
	   $(TARGET_CXX) $(OOPS_STORMLIB_CXXFLAGS) -c -o "$$o" "$$s" || exit 1; objs="$$objs $$o"; done; \
	 echo "StormLib: compiled $$n sources"; \
	 a=$$(command -v $(AR) 2>/dev/null || command -v ar); "$$a" rcs $@ $$objs
	@echo "StormLib: $@"

.PHONY: stormlib-clean
stormlib-clean:
	@rm -rf $(OOPS_STORMLIB_BUILD)
