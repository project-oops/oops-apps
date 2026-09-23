# Header prerequisites: the headers a source reaches, made into prerequisites of what it is
# compiled into, so that a relink happens when one is due.
#
# # Why this exists
#
# **A source list is only half of a prerequisite list.** `app.mk` has named every `.c` a payload
# compiles as a prerequisite of its ELF since 2026-09-20, so a change to an SDK *source*
# relinked. A change to an SDK *header* did not. Nothing on this side knew that gl1-probe's 45
# sources reach 42 of the SDK's headers, so make compared the ELF against the sources alone,
# found it newer than every one of them, and did nothing.
#
# The shape of that failure is the expensive one, because **every step reports success**. The
# link does not run; `selfish` faithfully wraps the ELF that was already there; `pros restore`
# answers `0 files, 0 B ... unchanged - not re-sent`; and the console keeps running the previous
# code with nothing anywhere saying so. Measured on 2026-09-23 with the source half already in
# place: `touch oops-sdk/include/GL/gl.h` then `make -n title` planned **no** link at all, while
# touching a `.c` planned one. That is why a one-constant change reached `gl2-probe`, whose own
# source had also changed, and not `gl1-probe`.
#
# It is the same defect the Mesa archives hit on 2026-09-17 and `CORE_SDK_SRCS` hit on
# 2026-09-20, one level further down: an input to the build that make could not see. Both of
# those were fixed by naming the input. This one cannot be.
#
# A list of headers per source is a second thing to keep in step with the first, and this
# repository has already paid for one of those: `OOPS_GL_SRCS` exists in `app.mk` because every
# app carried its own copy of the GL source list and they broke one at a time, as each was next
# built. A header list would be worse. There are 42 behind gl1-probe alone, they change whenever
# anyone writes an `#include`, and an omission has no symptom - it is the *absence* of a
# rebuild, which looks exactly like a build that had nothing to do. So the compiler is asked,
# from the same flags the real compile uses, and the answer cannot drift from the build it
# describes.
#
# # Two shapes, and which one a rule wants
#
# **Object rules** are the better answer and the default. Each source compiles to its own `.o`
# with `-MMD -MP`, the depfiles are read back, and a change recompiles only what actually
# included it. Measured on gl1-probe's 45 sources, under WSL `oops-builder`:
#
#                        one command   objects   objects -j32
#     from scratch            8.1 s      8.4 s       2.8 s
#     one SDK source          7.9 s      2.1 s       2.0 s
#     GL/gl.h (23 of them)    7.9 s      5.4 s       2.1 s
#     nothing changed         0.3 s      1.5 s          -
#
# Two things to read out of that. `-j` does **nothing** for one command and most of the work for
# forty-five, which is the larger half of the gain and was not available at all before. And the
# do-nothing build costs a second more, because make now stats 45 objects, 45 depfiles and the
# headers behind them across a Windows filesystem - the one case that got worse, and the one
# where nothing happens anyway.
#
#   MY_OBJS := $(call oops_objs,$(MY_BUILD)/obj,$(MY_SRCS))
#   -include $(MY_OBJS:.o=.d)
#   $(call oops_obj_rules,$(MY_BUILD)/obj,TARGET_CC,MY_CFLAGS,$(MY_SRCS))
#
#   $(MY_LIB): $(MY_OBJS)
#           ...link or archive $(MY_OBJS)...
#
# They need the source list to be **known when the rules are read**, because that is when the
# objects are named. That is true of `app.mk`, which every app includes last.
#
# It is not true everywhere. `common/cxx.mk` is included *before* a title sets `OOPS_CXX_SRCS` -
# Extreme Tux Racer does exactly that - and its recipe compiles `$(OOPS_CXX_SRCS)` expanded at
# recipe time, which is the only reason that archive contains anything at all. Naming objects at
# read time there would produce an empty list and `ar` would cheerfully write an empty archive.
# For a rule of that shape, a **whole-program depfile** is correct: one preprocessing pass over
# the sources, run from inside the recipe, where the list is finally known.
#
#   MY_DEPFILE := $(MY_BUILD)/libmine.a.d
#   -include $(MY_DEPFILE)
#
#   $(MY_LIB): $(MY_SRCS) $(MAKEFILE_LIST)
#           $(call oops_depgen,$(TARGET_CC),$(MY_CFLAGS),$@,$(MY_SRCS),$(MY_DEPFILE))
#           ...compile and archive...
#
# Note which arguments are values and which are names: `oops_depgen` runs inside a recipe and
# takes values, while `oops_obj_rules` writes a recipe and takes **variable names** -
# `TARGET_CC`, not `$(TARGET_CC)` - so the generated recipe looks them up when it runs and a
# title that appends to its flags afterwards still gets them.
#
# # The flags
#
# **`-MMD`/`-MM`, not `-MD`/`-M`.** They leave out the compiler's own headers and the sysroot's.
# That is the right cut here: `oops-sdk`'s headers arrive through `-I` and are listed, while a
# hosted title's Mesa sysroot arrives through `-isystem` and is not - and the sysroot is pinned,
# with its archives already named in `PAYLOAD_EXTRA_DEPS`.
#
# **`-MP`.** Without it, deleting or renaming a header stops the *next* build with
# `No rule to make target '.../old.h'`: the depfile still names a file nothing can produce, and
# the error is about the build system rather than about anything the porter did. `-MP` writes a
# bare rule for each header, which make reads as "gone, so rebuild", which is the right answer.
#
# **`-MT`**, in the whole-program form, names the real target. Without it each rule lands on a
# `.o` that form of the build never produces, and the depfile is inert - the quietest possible
# way for this whole file to not work.
#
# **`-Qunused-arguments`**, in the whole-program form only, because there the flags are a *link*
# command's: that shape compiles and links in one invocation, so its `CFLAGS` carry `-nostdlib`
# and its kin. A preprocess-only run can call those unused, and `-Werror` would turn that into a
# failed build for no reason anyone could act on.
#
# **Written through a temporary**, in the whole-program form, because a pass that is interrupted
# part way would otherwise leave a truncated depfile behind - and a truncated depfile is a
# *shorter* prerequisite list that make has every reason to believe. `-MMD` writes each object's
# depfile as part of compiling it and has nothing to half-write.
#
# # Where the objects go
#
# Under `<objdir>/`, at a path derived from the source's own, so that `gl_draw.c` from the SDK
# and a `gl_draw.c` in an app cannot land on the same `.o`. The roots come from **this file's
# own location**, the way every script in the collection derives its roots, so the mapping does
# not depend on which variables happen to be set when this is included:
#
#   <objdir>/app/...   the app's own directory ($(CURDIR))
#   <objdir>/apps/...  elsewhere in oops-apps - common/posix, src/oops-deps
#   <objdir>/oops/...  elsewhere in the collection - oops-sdk, oops-mesa
#   <objdir>/ext/...   anything else, by absolute path. Nothing reaches it today.
#
# A host self-test and a payload compile many of the *same files* under a different compiler and
# a different set of flags, so they are given different `<objdir>`s - `build/obj` and
# `build/hostobj`. Sharing one would mean whichever built last decided what the other linked.

ifndef OOPS_DEPS_MK
OOPS_DEPS_MK := 1

OOPS_DEPS_DIR  := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
OOPS_DEPS_APPS := $(abspath $(OOPS_DEPS_DIR)/..)
OOPS_DEPS_OOPS := $(abspath $(OOPS_DEPS_DIR)/../..)

# **`$(MAKEFILE_LIST)` is not the list of makefiles once depfiles are being read.**
#
# A depfile is included with `-include`, and `-include` is how you read a makefile, so make
# appends every one of them to `MAKEFILE_LIST` alongside `app.mk` and the rest. Naming that list
# as a prerequisite - which the rules here do, because a flag change is a reason to recompile -
# then makes **every depfile a prerequisite of every object**. Each compile rewrites its own
# depfile, that depfile is now newer than the other forty-four objects, and the next build
# recompiles all of them. Forever.
#
# It is a particularly good disguise: the build is correct, it is merely never incremental, so
# it looks exactly like "object rules did not help" rather than like a bug. Caught here on
# 2026-09-23 by `make --debug=b`, which said in as many words that `gl_draw.d` was newer than
# `gl_list.o`.
#
# Use this instead of `$(MAKEFILE_LIST)` in any rule that names the makefiles as prerequisites.
oops_makefiles = $(filter-out %.d,$(MAKEFILE_LIST))

# $(call oops_obj_key,<absolute source>) -> the object's path below <objdir>, suffix and all.
# Deepest root first, because $(CURDIR) is inside oops-apps and oops-apps is inside the
# collection.
oops_obj_key = $(or \
    $(patsubst $(CURDIR)/%,app/%,$(filter $(CURDIR)/%,$(1))), \
    $(patsubst $(OOPS_DEPS_APPS)/%,apps/%,$(filter $(OOPS_DEPS_APPS)/%,$(1))), \
    $(patsubst $(OOPS_DEPS_OOPS)/%,oops/%,$(filter $(OOPS_DEPS_OOPS)/%,$(1))), \
    ext$(1))

# $(call oops_obj,<objdir>,<source>)   -> one object path
# $(call oops_objs,<objdir>,<sources>) -> the object list, in the order the sources were given,
#                                         which is the order they reach the linker
oops_obj  = $(1)/$(basename $(call oops_obj_key,$(abspath $(2)))).o
oops_objs = $(foreach s,$(2),$(call oops_obj,$(1),$(s)))

# $(call oops_obj_rules,<objdir>,<CC var name>,<CFLAGS var name>,<sources>)
#
# One rule per source. `$(sort)` because it also removes duplicates, and two rules for one target
# is a make warning nobody reads. The makefiles are prerequisites for the reason they are on the
# link: a flag is an input to the build too, and changing one would otherwise leave every object
# compiled under the old flags sitting there looking finished. `$(oops_makefiles)` rather than
# `$(MAKEFILE_LIST)`, for the reason given where it is defined.
define oops_obj_rule_one
$(call oops_obj,$(1),$(4)): $(4) $$(oops_makefiles)
	@mkdir -p $$(@D)
	$$($(2)) $$($(3)) -MMD -MP -c -o $$@ $(4)
endef
oops_obj_rules = $(foreach s,$(sort $(4)),$(eval $(call oops_obj_rule_one,$(1),$(2),$(3),$(s))))

# $(call oops_depgen,<compiler>,<compile flags>,<target>,<sources>,<depfile>)
#
# The whole-program form, for a rule whose sources are only known once its recipe runs. Writes
# one `<target>: header ...` rule per source into <depfile>; make unions those with the
# prerequisites the rule states itself.
oops_depgen = @$(1) $(2) -Qunused-arguments -MM -MP -MT $(3) $(4) > $(5).tmp && mv -f $(5).tmp $(5)

endif
