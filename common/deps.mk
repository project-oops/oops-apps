# Header prerequisites, asked of the compiler rather than kept by hand.
#
# Include it before the rule that needs it, and use it in two places - the `-include` and the
# first line of the recipe:
#
#   include $(OOPS_APPS_ROOT)/common/deps.mk
#
#   MY_DEPFILE := $(MY_BUILD)/libmine.a.d
#   -include $(MY_DEPFILE)
#
#   $(MY_LIB): $(MY_SRCS) $(MAKEFILE_LIST)
#           $(call oops_depgen,$(TARGET_CC),$(MY_CFLAGS),$@,$(MY_SRCS),$(MY_DEPFILE))
#           ...the compile...
#
# # Why this exists
#
# **A source list is only half of a prerequisite list.** `app.mk` has named every `.c` a payload
# compiles as a prerequisite of its ELF since 2026-09-20, so a change to an SDK *source* relinks.
# A change to an SDK *header* did not. Nothing on this side knew that gl1-probe's 45 sources
# reach 42 of the SDK's headers, so make compared the ELF against the sources alone, found it
# newer than every one of them, and did nothing.
#
# The shape of that failure is the expensive one, because **every step reports success**. The
# link does not run; `selfish` faithfully wraps the ELF that was already there; `pros restore`
# answers `0 files, 0 B ... unchanged - not re-sent`; and the console keeps running the previous
# code with nothing anywhere saying so. Measured on 2026-09-23, with the source half already in
# place: a one-constant change in `oops-sdk/src/gl/gl_draw.c` reached `gl2-probe`, whose own
# source had also changed, and not `gl1-probe` - and a `clean` rebuild of gl1-probe then produced
# a different `eboot.bin`, which is how the "successful" incremental build was shown to have done
# nothing at all.
#
# It is the same defect the Mesa archives hit on 2026-09-17 and `CORE_SDK_SRCS` hit on
# 2026-09-20, one level further down: an input to the build that make could not see. Both of
# those were fixed by naming the input. This one cannot be, which is the rest of this file.
#
# # Why the compiler is asked, and not a list kept here
#
# A list of headers per source is a second thing to keep in step with the first, and this
# repository has already paid for one of those: `OOPS_GL_SRCS` exists in `app.mk` because every
# app carried its own copy of the GL source list and they broke one at a time, as each was next
# built. A header list would be worse. There are 42 behind gl1-probe alone, they change whenever
# anyone writes an `#include`, and an omission has no symptom - it is the *absence* of a rebuild,
# which looks exactly like a build that had nothing to do.
#
# The compiler resolves them from the same flags the real compile uses, so the answer cannot
# drift from the build it describes, and no app has to know that any of this happened.
#
# # The flags, each of which is load-bearing
#
# **`-MM`, not `-M`.** `-MM` leaves out the compiler's own headers and the sysroot's. That is the
# right cut here: `oops-sdk`'s headers arrive through `-I` and are listed, while a hosted title's
# Mesa sysroot arrives through `-isystem` and is not - and the sysroot is pinned, with its
# archives already named in `PAYLOAD_EXTRA_DEPS`.
#
# **`-MP`.** Without it, deleting or renaming a header stops the *next* build with
# `No rule to make target '.../old.h'`: the depfile still names a file nothing can produce, and
# the error is about the build system rather than about anything the porter did. `-MP` writes a
# bare rule for each header, which make reads as "gone, so rebuild", which is the right answer.
#
# **`-MT`** names the real target. Without it each rule lands on a `.o` this build never produces
# and the depfile is inert - the quietest possible way for this whole file to not work.
#
# **`-Qunused-arguments`** because the compile flags here are a *link* command's flags: `app.mk`
# compiles and links in one invocation, so `TARGET_CFLAGS` carries `-nostdlib` and its kin. A
# preprocess-only run can call those unused, and `-Werror` would turn that into a failed build
# for no reason anyone could act on.
#
# **Written through a temporary.** A pass that is interrupted or that fails part way would
# otherwise leave a truncated depfile behind, and a truncated depfile is a *shorter* prerequisite
# list that make has every reason to believe.
#
# # What it costs
#
# One preprocessor pass over the sources, on the builds that were about to compile them anyway:
# 0.2 s for gl1-probe's 45 sources, against 3.6 s for the compile and link that follows. A build
# with nothing to do stays a build with nothing to do, which is the whole reason for doing this
# rather than marking the link `.PHONY`.

ifndef OOPS_DEPS_MK
OOPS_DEPS_MK := 1

# $(call oops_depgen,<compiler>,<compile flags>,<target>,<sources>,<depfile>)
#
# Writes one `<target>: header ...` rule per source into <depfile>. Make unions those with the
# prerequisites the rule states itself, so the sources stay where a reader can see them and only
# the headers - which no reader could keep accurate - come from here.
oops_depgen = @$(1) $(2) -Qunused-arguments -MM -MP -MT $(3) $(4) > $(5).tmp && mv -f $(5).tmp $(5)

endif
