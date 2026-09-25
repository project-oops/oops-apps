# cxx-throw - does a C++ exception survive a round trip on this platform?
#
#   make check      run the five checks against the build machine's own C++ runtime, then
#                   verify the target ELF really carries an unwinder (any machine)
#   make elf        the plain-ELF payload a homebrew loader runs on the console
#   make dist       stages conforming release artifact under dist/
#
# Configuration & release formats loaded from app.env
#
# # What this probe is for
#
# The platform exports **none** of the 19 Itanium unwind and C++ ABI symbols - obSCEne swept
# `libkernel`, `libSceLibcInternal` and `self` and every one came back absent at `0x0`
# (REQ-20260921T0953Z-e3f7). It runs an MSVC-lineage SEH unwinder, which is not the ABI clang
# emits. So a throwing title carries its own libunwind and libc++abi, and this is the smallest
# thing that says whether that works. (oops-apps#D005)

OOPS_APPS_ROOT ?= $(abspath ../../..)
OOPS_LIBCXX    ?= $(abspath ../../oops-deps/libcxx)

# The C++ side, built into an archive by `cxx.mk` - see its header for why an archive and not
# sources in PAYLOAD_SRCS.
OOPS_CXX_EXCEPTIONS = 1
OOPS_CXX_SRCS       = boundary.cpp probe.cpp

include $(OOPS_LIBCXX)/oops-libcxx.mk
include $(OOPS_LIBCXX)/oops-libunwind.mk
include $(OOPS_LIBCXX)/oops-libcxxabi.mk
include $(OOPS_APPS_ROOT)/common/cxx.mk

OOPS_CXX_INCLUDE     += $(OOPS_LIBCXX_INCLUDE)
EXTRA_TARGET_LDFLAGS += $(OOPS_CXX_LDFLAGS) $(OOPS_LIBCXXABI_LDFLAGS) $(OOPS_LIBUNWIND_LDFLAGS)

# **`OOPS_CXX_INCLUDE` deliberately does NOT go into `EXTRA_TARGET_CFLAGS`**, although `cxx.mk`'s
# own header shows that line. It is right for a title whose C sources include C++-adjacent
# headers and wrong here, and getting it wrong is loud in a confusing way: libc++'s include
# directory carries its own `<stddef.h>`, so putting it on the C payload compile makes
# oops-sdk's `<stddef.h>` resolve to libc++'s, and every SDK C source then fails inside
# `__config`. This app's C side includes no C++ headers, so it wants none of them.
PAYLOAD_EXTRA_DEPS   += $(OOPS_CXX_LIB) $(OOPS_LIBCXXABI_LIB) $(OOPS_LIBUNWIND_LIB)

# The link order matters and is not alphabetical: libc++abi calls into libunwind, so the
# unwinder comes last. Both are `--whole-archive` (their own fragments say why): the personality
# routine and the `_Unwind_*` entry points are reached from exception tables rather than from
# calls, so on-demand extraction takes neither.

# Target payload sources. No display, no input: the question is answered by a number on klog.
PAYLOAD_SRCS = cxx_throw_main.c

ENTRY_POINT := cxx_throw_start

include $(OOPS_APPS_ROOT)/common/app.mk

# # The host check, and why it is built by hand here
#
# `app.mk`'s `HOST_TEST_SRCS` compiles with `$(CC) -std=c11`, which cannot build a C++ probe.
# Rather than teach `app.mk` about a second language for one app, this app builds its own host
# binary with `clang++` against the *build machine's* C++ runtime.
#
# **That is the point of it, not a compromise.** obSCEne's fifth rule for adding a check is to
# run it against a known-good implementation before believing anything it says, and this is
# that: if a check fails here, the check is wrong, because the build machine's libstdc++
# unwinds correctly. It says nothing about the console and is not meant to.
HOST_PROBE := $(BUILD)/cxx-throw_hostprobe

$(HOST_PROBE): probe.cpp boundary.cpp cxx_throw.h | $(BUILD)
	@clang++ -std=c++17 -O1 -Wall -Wextra -o $@ \
	    host_main.cpp probe.cpp boundary.cpp
	@echo "cxx-throw: host probe built"

.PHONY: check-host check-target-symbols check-target-eh-frame check-target-frames \
        check-target-eh-readable
check-host: $(HOST_PROBE)
	@$(HOST_PROBE)

# # The check that actually matters, and why a clean link is not it
#
# `app.mk` links with `-Wl,--unresolved-symbols=ignore-all`. So a payload that failed to link an
# unwinder **still links**, and every one of these symbols resolves to zero at run time. The
# build succeeding is therefore no evidence at all; the symbol table is.
#
# This is the same discipline oops-sdk records for cross-module calls, arrived at the same way:
# a clean exit code proving nothing.
#
# ## The map, not the ELF
#
# This read `nm $(BUILD)/$(APP_NAME).elf` until 2026-09-23 and reported all eight symbols
# MISSING on a perfectly good build. `make title` rewrites that ELF in place into a console
# module - its tables move into an unmapped vendor segment - so after a full build `nm` finds
# one line and the check is measuring the packaging step rather than the link.
#
# `$(BUILD)/$(APP_NAME).map` is written by the linker and nothing rewrites it afterwards, so it
# answers the same question in the state the question is about.
#
# Both checks still *depend* on the ELF, because the map has no rule of its own - `app.mk`
# writes it as a side effect of the link (`-Wl,-Map=`), so the ELF's rule is what produces it.
#
# ## Two line shapes, and the pattern has to admit both
#
# A symbol from an object file ends the line with its name:
#
#             20bd0            20bd0       45     1                 __cxa_throw
#
# A symbol *assigned by a linker script* - which is what all four `__eh_frame_*` are - ends it
# with the assignment instead:
#
#             3b7a8            3b7a8        0     1         __eh_frame_start = .
#
# Matching only the first shape is how the first pass of this rewrite reported the two symbols
# it was written to check as MISSING while the other six passed.
MAP := $(BUILD)/$(APP_NAME).map

# # The two ELF-reading checks relink first, because `check` packages before they run
#
# `make title` rewrites `$(BUILD)/$(APP_NAME).elf` in place into a console module and drops its
# section headers, and `check` reaches the packaging step first. So a check that only *detected*
# the stripped ELF failed every full `make check` - correctly, and uselessly.
#
# Relinking is the cheap answer: same inputs, same output, about ten seconds, and it leaves the
# packaged title alone under `build/title/`. The two checks that read sections depend on this;
# the two that read the map do not, because nothing rewrites the map.
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

# # Present is not the same as non-empty
#
# The loop above passed on a payload whose every `throw` reached std::terminate. All four
# `__eh_frame_*` symbols were defined and non-zero - and `start` and `end` held the *same*
# address, because the base linker script listed `*(.eh_frame .eh_frame_hdr)` in `/DISCARD/`
# and a discard beats the `KEEP` in `oops-deps/libcxx/link/eh-frame.ld`.
#
# A freestanding libunwind takes that range as its whole frame table. Zero bytes of table means
# it cannot walk out of the frame that threw, so it never reaches the handler tables at all and
# `catch (...)` is as unreachable as a typed catch - which is what made this look like a
# personality-routine fault rather than an empty table.
#
# So the width is the check, and it is separate from the presence above because the two fail
# for different reasons and want different answers.
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

# # And a wide table is not the same as a table covering the right frames
#
# 19,540 bytes of `.eh_frame` proves something was kept. It does not prove there is an FDE for
# the function that throws, for the one holding the `catch`, or for `__cxa_throw` in between -
# and an unwind walks all three. A table full of FDEs for libunwind's own frames and none for
# the probe's would measure the same width and fail the same way on hardware.
#
# So this asks the question directly: for each address the unwind has to step through, is there
# an FDE whose pc range contains it? `llvm-dwarfdump --eh-frame` prints the ranges; `nm` gives
# the addresses. Both come from this build, so nothing here is transcribed.
#
# ## Two absences that are correct, and are asserted as absences
#
# `_Unwind_Resume_or_Rethrow` has no FDE and must not: libunwind is compiled `-fno-exceptions`
# because it *implements* throwing rather than throwing, so its frames are not unwind targets.
# `cxx_throw_start` has none either - it is the entry point, and an exception reaching it is by
# definition uncaught. Listing them here as expected-absent means a future build that starts
# emitting FDEs for them gets noticed rather than silently absorbed.
#
# ## `nm -C`, because two of these names are C++
#
# `cxx_throw_raise` is `_Z15cxx_throw_raisei` in the symbol table and matching it literally
# found nothing. The lookup demangles and then compares up to the `(`, so a signature change
# does not have to be tracked here; `cxx_throw_run` and `__cxa_throw` are `extern "C"` and pass
# through either way.
#
# ## Why this one needs the ELF and the others do not
#
# The frame table lives in the ELF; only the *symbols* live in the map. `make title` rewrites
# `$(BUILD)/$(APP_NAME).elf` in place into a console module and drops its section headers, so
# this check must see a fresh link. It says so rather than reporting a false failure, which is
# the mistake the other two checks made until today.
# **`_Unwind_RaiseException` and `_Unwind_Resume` belong on the covered list**, and putting them
# there is the whole lesson of 2026-09-23. Not throwing is not the same as not being unwound
# through: phase 1's first act is a DWARF step *past `_Unwind_RaiseException`'s own frame* -
# upstream's comment says "skip over first which is `_Unwind_RaiseException`" - and phase 2's
# cleanup path steps past `_Unwind_Resume`. libunwind was built without asynchronous unwind
# tables, so neither had an FDE, the first step returned end-of-stack, phase 1 concluded there
# were no frames to search, and `catch (...)` was unreachable. Three hardware probes read as a
# handler-table fault before this was the thing that was wrong.
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

# # A table you cannot read is worth exactly as much as one that is not there
#
# This platform maps a title's text **execute-only**: the CPU fetches instructions from it and
# may not load from it. The frame table is read as data, so it has to live in the readable
# segment - and until 2026-09-23 `oops-deps/libcxx/link/eh-frame.ld` said `INSERT AFTER .text`,
# which put it in the wrong one. Every check above passed on that build. The table was present,
# 19,540 bytes wide, and covered every frame an unwind walks. It was also unreadable, and the
# kernel killed the title on the unwinder's first load from it:
#
#     exception: 0xa0020328 (SYSTEM_XO_VIOLATION)
#     fault address: 00000000004403fc (xotext:"eboot.bin"+0x403fc)
#
# `0x403fc` is `__eh_frame_end` / `__eh_frame_hdr_start` exactly - the index's first byte.
#
# So this asks the only question the others cannot: **is the frame table in the same segment as
# `.text`?** If it is, it is execute-only and no unwind can read it. The positive half - that it
# sits with `.gcc_except_table` - is asserted too, because that is where the *other* unwind table
# already lives and their being apart is what disguised this for two hardware probes: the LSDA
# was always readable, so `catch (...)` failing looked like a personality-routine fault.
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
