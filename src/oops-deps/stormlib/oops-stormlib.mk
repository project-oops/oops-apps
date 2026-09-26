# StormLib build integration.
#
#   OOPS_STORMLIB ?= $(abspath $(OOPS_APPS_ROOT)/src/oops-deps/stormlib)
#   include $(OOPS_STORMLIB)/oops-stormlib.mk
#   EXTRA_TARGET_CFLAGS += $(OOPS_STORMLIB_INCLUDE)
#   EXTRA_LDFLAGS       += $(OOPS_STORMLIB_LDFLAGS)
#
# MPQ archive reading; Ship of Harkinian keeps a title's extracted assets in one. Needs zlib, so
# include `oops-zlib.mk` first (see `__SYS_ZLIB` below).
ifndef OOPS_STORMLIB_DIR
OOPS_STORMLIB_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
endif
OOPS_STORMLIB_UPSTREAM ?= $(OOPS_STORMLIB_DIR)/upstream
OOPS_STORMLIB_BUILD ?= $(OOPS_STORMLIB_DIR)/build

OOPS_STORMLIB_INCLUDE := -I$(OOPS_STORMLIB_UPSTREAM)/src
OOPS_STORMLIB_LIB := $(OOPS_STORMLIB_BUILD)/libstorm.a
OOPS_STORMLIB_LDFLAGS := $(OOPS_STORMLIB_LIB)

# Upstream builds the library from `SRC_FILES`, `TOMCRYPT_FILES`, `TOMMATH_FILES` and, with no
# system bzip2, `BZIP2_FILES` (`CMakeLists.txt`). A payload link does not report undefined
# symbols, so check the archive with `nm --undefined-only` after a bump.
#
# The extra lists are whole directories, globbed below with their counts asserted.
#
# C and C++ sources are separate because each compiles with its own front end; the `.c` files
# are C and do not compile as C++.
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

# The crypto pair, for `SFileVerify`. `TOMCRYPT_FILES` and the libtomcrypt files in `SRC_FILES`
# together are every `.c` under the tree, so the glob is their union.
OOPS_STORMLIB_TOMCRYPT_SRCS := $(shell find $(OOPS_STORMLIB_UPSTREAM)/src/libtomcrypt -name '*.c')
OOPS_STORMLIB_TOMMATH_SRCS  := $(shell find $(OOPS_STORMLIB_UPSTREAM)/src/libtommath -name '*.c')
# Bundled bzip2: MPQ compression method 0x10 is bzip2, dispatched from a table in
# `SCompression.cpp`, so any archive can reach it.
OOPS_STORMLIB_BZIP2_SRCS    := $(shell find $(OOPS_STORMLIB_UPSTREAM)/src/bzip2 -name '*.c')
OOPS_STORMLIB_C_SRCS += $(OOPS_STORMLIB_TOMCRYPT_SRCS) $(OOPS_STORMLIB_TOMMATH_SRCS) \
                        $(OOPS_STORMLIB_BZIP2_SRCS)

# Upstream's counts at the pinned revision; a bump that adds or drops a file fails here.
OOPS_STORMLIB_EXPECT := 78:$(words $(OOPS_STORMLIB_TOMCRYPT_SRCS)):libtomcrypt \
                        118:$(words $(OOPS_STORMLIB_TOMMATH_SRCS)):libtommath \
                        7:$(words $(OOPS_STORMLIB_BZIP2_SRCS)):bzip2
$(foreach e,$(OOPS_STORMLIB_EXPECT),\
  $(if $(filter $(word 1,$(subst :, ,$(e))),$(word 2,$(subst :, ,$(e)))),,\
    $(error StormLib: $(word 3,$(subst :, ,$(e))) has $(word 2,$(subst :, ,$(e))) sources, \
            upstream's CMakeLists names $(word 1,$(subst :, ,$(e))) - re-check oops-stormlib.mk \
            against CMakeLists.txt after this bump)))

# `__SYS_ZLIB` uses `oops-deps/zlib` instead of the bundled copy; both define `inflate`, and a
# payload link resolves a duplicate symbol without complaint.
#
# `_7ZIP_ST` is LZMA's single-threaded build; the alternative wants Windows threads.
#
# `__PROSPERO__` is what `patches/0001` keys on to skip the `sys/mman.h` arm of upstream's
# `StormPort.h:309`, so it is set for every source.
#
# `BZ_STRICT_ANSI` is upstream's `add_definitions`; it keeps bzip2's command-line file handling out.
OOPS_STORMLIB_DEFS := -D__SYS_ZLIB -D_7ZIP_ST -DBZ_STRICT_ANSI -D__PROSPERO__=1
OOPS_STORMLIB_TARGET = -target x86_64-unknown-freebsd -ffreestanding -fno-builtin -nostdlib \
                       -fPIC -O2 -w
OOPS_STORMLIB_OWN    = $(OOPS_STORMLIB_INCLUDE) $(OOPS_STORMLIB_DEFS) $(OOPS_ZLIB_INCLUDE)
OOPS_STORMLIB_CFLAGS = $(OOPS_STORMLIB_TARGET) -nostdlibinc -std=gnu11 $(OOPS_STORMLIB_OWN) \
                       $(OOPS_POSIX_INCLUDE) $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE)
# `$(OOPS_LIBCXX_INCLUDE)` comes before the C headers: `<cmath>` requires libc++'s wrapping
# `<math.h>` ahead of the C library's. It also carries `-nostdinc++ -nostdlibinc`.
OOPS_STORMLIB_CXXFLAGS = $(OOPS_STORMLIB_TARGET) -std=c++17 $(OOPS_STORMLIB_OWN) \
                         $(OOPS_LIBCXX_INCLUDE) \
                         $(OOPS_POSIX_INCLUDE) $(OOPS_SDK_INCLUDE) $(OOPS_SDK_LIBC_INCLUDE)

# Objects are named by a counter rather than by basename, so sources with the same basename in
# different directories cannot overwrite each other in the flat build directory.
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
