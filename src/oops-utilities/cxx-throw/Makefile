# cxx-throw - does a C++ exception survive a round trip on this platform?
#
#   make check      run the five checks against the build machine's own C++ runtime, then
#                   verify the target ELF really carries an unwinder (any machine)
#   make elf        the plain-ELF payload a homebrew loader runs on the console
#   make dist       stages conforming release artifact under dist/
#
# Configuration & release formats loaded from app.env
#
# The platform exports no Itanium unwind or C++ ABI symbols, so a throwing title links its
# own libunwind and libc++abi (oops-apps#D005). This is the smallest title that does.

OOPS_APPS_ROOT ?= $(abspath ../../..)
OOPS_LIBCXX    ?= $(abspath ../../oops-deps/libcxx)

# The C++ side, built into an archive by `cxx.mk`.
OOPS_CXX_EXCEPTIONS = 1
OOPS_CXX_SRCS       = boundary.cpp probe.cpp

include $(OOPS_LIBCXX)/oops-libcxx.mk
include $(OOPS_LIBCXX)/oops-libunwind.mk
include $(OOPS_LIBCXX)/oops-libcxxabi.mk
include $(OOPS_APPS_ROOT)/common/cxx.mk

OOPS_CXX_INCLUDE     += $(OOPS_LIBCXX_INCLUDE)
EXTRA_TARGET_LDFLAGS += $(OOPS_CXX_LDFLAGS) $(OOPS_LIBCXXABI_LDFLAGS) $(OOPS_LIBUNWIND_LDFLAGS)

# `OOPS_CXX_INCLUDE` stays out of `EXTRA_TARGET_CFLAGS`: libc++'s `<stddef.h>` would shadow
# oops-sdk's in the C payload compile, and this app's C side includes no C++ headers.
PAYLOAD_EXTRA_DEPS   += $(OOPS_CXX_LIB) $(OOPS_LIBCXXABI_LIB) $(OOPS_LIBUNWIND_LIB)

# libc++abi calls into libunwind, so the unwinder links last. Both are `--whole-archive`:
# exception tables, not calls, reach the personality routine and `_Unwind_*`.

# Target payload sources. No display or input; the result is a line on klog.
PAYLOAD_SRCS = cxx_throw_main.c

ENTRY_POINT := cxx_throw_start

include $(OOPS_APPS_ROOT)/common/app.mk

# The host check runs the probe against the build machine's C++ runtime, a known-good
# unwinder, so a failure there means the probe is wrong. Built here with `clang++` because
# `app.mk`'s HOST_TEST_SRCS compiles C only.
HOST_PROBE := $(BUILD)/cxx-throw_hostprobe

$(HOST_PROBE): probe.cpp boundary.cpp cxx_throw.h | $(BUILD)
	@clang++ -std=c++17 -O1 -Wall -Wextra -o $@ \
	    host_main.cpp probe.cpp boundary.cpp
	@echo "cxx-throw: host probe built"

.PHONY: check-host check-target-symbols check-target-eh-frame check-target-frames \
        check-target-eh-readable
check-host: $(HOST_PROBE)
	@$(HOST_PROBE)

# The link ignores unresolved symbols, so the symbol checks read the linker map. `make title`
# rewrites the ELF into a console module; the map is written by the link (`-Wl,-Map=`) and
# never rewritten, so the checks depend on the ELF rule that produces it. A map line ends
# with the symbol name, or with `name = .` for a linker-script assignment like
# `__eh_frame_start`; the pattern admits both.
MAP := $(BUILD)/$(APP_NAME).map

# `make title` drops the ELF's section headers, and `check` packages first, so the two
# section-reading checks relink the ELF when it has none. The packaged title is untouched.
.PHONY: relink-if-stripped
relink-if-stripped: $(BUILD)/$(APP_NAME).elf
	@if [ "$$(readelf -hW $(BUILD)/$(APP_NAME).elf 2>/dev/null \
	          | awk '/Number of section headers/ {print $$NF}')" = "0" ]; then \
	    echo "cxx-throw: the ELF was packaged into a console module; relinking to read sections"; \
	    rm -f $(BUILD)/$(APP_NAME).elf; \
	    $(MAKE) --no-print-directory $(BUILD)/$(APP_NAME).elf >/dev/null; \
	fi

check-target-symbols: $(BUILD)/$(APP_NAME).elf
	@missing=0; \
	for s in __cxa_throw __cxa_begin_catch __cxa_allocate_exception __gxx_personality_v0 \
	         _Unwind_RaiseException _Unwind_Resume \
	         __eh_frame_start __eh_frame_end; do \
	    if grep -qE "[[:space:]]$$s([[:space:]]*=.*)?\$$" $(MAP) 2>/dev/null; then \
	        echo "  ok       $$s"; \
	    else \
	        echo "  MISSING  $$s"; missing=$$((missing+1)); \
	    fi; \
	done; \
	if [ "$$missing" -ne 0 ]; then \
	    echo "cxx-throw: $$missing unwinder symbol(s) absent - the payload would fault on its first throw" >&2; \
	    exit 1; \
	fi; \
	echo "cxx-throw: the payload carries its own unwinder"

# The frame table is non-empty. A freestanding libunwind takes `__eh_frame_start..end` as
# its whole table; with zero bytes it cannot leave the throwing frame and no catch runs. A
# `/DISCARD/` of `.eh_frame` in the base linker script beats the `KEEP` in
# `oops-deps/libcxx/link/eh-frame.ld`.
check-target-eh-frame: $(BUILD)/$(APP_NAME).elf
	@start=$$(grep -E "[[:space:]]__eh_frame_start([[:space:]]*=.*)?\$$" $(MAP) | awk '{print $$1}' | tail -1); \
	end=$$(grep -E "[[:space:]]__eh_frame_end([[:space:]]*=.*)?\$$" $(MAP) | awk '{print $$1}' | tail -1); \
	if [ -z "$$start" ] || [ -z "$$end" ]; then \
	    echo "cxx-throw: __eh_frame_start/__eh_frame_end are not in $(MAP)" >&2; \
	    exit 1; \
	fi; \
	bytes=$$(( 0x$$end - 0x$$start )); \
	if [ "$$bytes" -le 0 ]; then \
	    echo "cxx-throw: the frame table is $$bytes bytes (0x$$start .. 0x$$end)" >&2; \
	    echo "  every throw in this payload would reach std::terminate without entering a catch," >&2; \
	    echo "  because with no frame table the unwinder cannot leave the frame that threw." >&2; \
	    echo "  Check the base linker script does not discard .eh_frame - see" >&2; \
	    echo "  oops-deps/libcxx/link/eh-frame.ld and selfish/link/native_eboot.ld." >&2; \
	    exit 1; \
	fi; \
	echo "  ok       frame table is $$bytes bytes (0x$$start .. 0x$$end)"

# Every frame an unwind steps through has an FDE whose pc range contains it
# (`llvm-dwarfdump --eh-frame` ranges against `nm -C` addresses, compared up to the `(`).
# Phase 1 steps past `_Unwind_RaiseException`'s own frame and phase 2 cleanup past
# `_Unwind_Resume`, so both need FDEs. `cxx_throw_start` is the entry point and has none by
# design. Reads the ELF's sections, so it relinks a packaged ELF first.
COVERED_BY_FDE   := cxx_throw_raise cxx_throw_raise_derived cxx_throw_run __cxa_throw \
                    __cxa_begin_catch __gxx_personality_v0 \
                    _Unwind_RaiseException _Unwind_Resume
UNCOVERED_BY_FDE := cxx_throw_start

check-target-frames: relink-if-stripped
	@llvm-dwarfdump --eh-frame $(BUILD)/$(APP_NAME).elf 2>/dev/null \
	    | grep -oE 'pc=[0-9a-f]+\.\.\.[0-9a-f]+' | sed 's/pc=//; s/\.\.\./ /' > $(BUILD)/.fdes; \
	total=$$(wc -l < $(BUILD)/.fdes); \
	if [ "$$total" -eq 0 ]; then \
	    echo "cxx-throw: the frame table parses to no FDEs at all" >&2; exit 1; \
	fi; \
	bad=0; \
	for s in $(COVERED_BY_FDE) $(UNCOVERED_BY_FDE); do \
	    want=covered; \
	    case " $(UNCOVERED_BY_FDE) " in *" $$s "*) want=absent;; esac; \
	    a=$$(nm -C --defined-only $(BUILD)/$(APP_NAME).elf 2>/dev/null \
	         | awk -v n="$$s" '{ f = $$3; sub(/\(.*/, "", f); if (f == n) { print $$1; exit } }'); \
	    if [ -z "$$a" ]; then echo "  MISSING  $$s is not in the symbol table"; bad=$$((bad+1)); continue; fi; \
	    hit=$$(awk -v a="$$((0x$$a))" '{ if (a >= strtonum("0x" $$1) && a < strtonum("0x" $$2)) \
	                                     { printf "%s..%s", $$1, $$2; exit } }' $(BUILD)/.fdes); \
	    if [ "$$want" = covered ] && [ -n "$$hit" ]; then \
	        echo "  ok       $$s covered by FDE $$hit"; \
	    elif [ "$$want" = absent ] && [ -z "$$hit" ]; then \
	        echo "  ok       $$s has no FDE, as it should not"; \
	    elif [ "$$want" = covered ]; then \
	        echo "  NO FDE   $$s at 0x$$a - an unwind through it cannot proceed"; bad=$$((bad+1)); \
	    else \
	        echo "  UNEXPECTED $$s now has an FDE ($$hit); it had none by design"; bad=$$((bad+1)); \
	    fi; \
	done; \
	rm -f $(BUILD)/.fdes; \
	if [ "$$bad" -ne 0 ]; then \
	    echo "cxx-throw: $$bad frame(s) wrong - see oops-deps/libcxx/link/eh-frame.ld" >&2; exit 1; \
	fi; \
	echo "cxx-throw: $$total FDEs, and every frame an unwind walks is covered"

# The frame table is readable as data. A title's text is execute-only, so a load from it
# faults with SYSTEM_XO_VIOLATION; `.eh_frame` and `.eh_frame_hdr` must sit outside the
# `.text` segment, in the same one as `.gcc_except_table`.
check-target-eh-readable: relink-if-stripped
	@map=$$(readelf -lW $(BUILD)/$(APP_NAME).elf 2>/dev/null | awk '/Section to Segment/,0'); \
	seg_text=$$(echo "$$map"  | awk '{ for (i=2;i<=NF;i++) if ($$i == ".text")             { print $$1; exit } }'); \
	seg_eh=$$(echo "$$map"    | awk '{ for (i=2;i<=NF;i++) if ($$i == ".eh_frame")         { print $$1; exit } }'); \
	seg_hdr=$$(echo "$$map"   | awk '{ for (i=2;i<=NF;i++) if ($$i == ".eh_frame_hdr")     { print $$1; exit } }'); \
	seg_lsda=$$(echo "$$map"  | awk '{ for (i=2;i<=NF;i++) if ($$i == ".gcc_except_table") { print $$1; exit } }'); \
	if [ -z "$$seg_text" ] || [ -z "$$seg_eh" ] || [ -z "$$seg_hdr" ] || [ -z "$$seg_lsda" ]; then \
	    echo "cxx-throw: could not place .text/.eh_frame/.eh_frame_hdr/.gcc_except_table in a segment" >&2; \
	    exit 1; \
	fi; \
	bad=0; \
	for pair in ".eh_frame:$$seg_eh" ".eh_frame_hdr:$$seg_hdr"; do \
	    s=$${pair%%:*}; n=$${pair##*:}; \
	    if [ "$$n" = "$$seg_text" ]; then \
	        echo "  XO       $$s is in segment $$n, the same one as .text" >&2; bad=$$((bad+1)); \
	    elif [ "$$n" != "$$seg_lsda" ]; then \
	        echo "  APART    $$s is in segment $$n, .gcc_except_table is in $$seg_lsda" >&2; bad=$$((bad+1)); \
	    else \
	        echo "  ok       $$s is in segment $$n with .gcc_except_table, not $$seg_text with .text"; \
	    fi; \
	done; \
	if [ "$$bad" -ne 0 ]; then \
	    echo "cxx-throw: the frame table is not readable as data - this platform's text is" >&2; \
	    echo "  execute-only, so an unwind through it dies on SYSTEM_XO_VIOLATION. Check the" >&2; \
	    echo "  INSERT AFTER target in oops-deps/libcxx/link/eh-frame.ld; it must follow a" >&2; \
	    echo "  section in the readable PT_LOAD (.rodata), never .text." >&2; \
	    exit 1; \
	fi

check: check-host check-target-symbols check-target-eh-frame check-target-frames check-target-eh-readable
