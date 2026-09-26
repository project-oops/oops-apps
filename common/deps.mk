# Header prerequisites: the headers a source reaches become prerequisites of what it is
# compiled into, as the compiler reports them from the real compile flags.
#
# Object rules are the default: each source compiles to its own `.o` with `-MMD -MP`, the
# depfiles are read back, and a change recompiles only what included it. They need the
# source list when the rules are read:
#
#   MY_OBJS := $(call oops_objs,$(MY_BUILD)/obj,$(MY_SRCS))
#   -include $(MY_OBJS:.o=.d)
#   $(call oops_obj_rules,$(MY_BUILD)/obj,TARGET_CC,MY_CFLAGS,$(MY_SRCS))
#
# A rule whose sources are known only at recipe time (`common/cxx.mk`) uses one
# whole-program depfile written from inside the recipe:
#
#   MY_DEPFILE := $(MY_BUILD)/libmine.a.d
#   -include $(MY_DEPFILE)
#   $(MY_LIB): $(MY_SRCS) $(MAKEFILE_LIST)
#           $(call oops_depgen,$(TARGET_CC),$(MY_CFLAGS),$@,$(MY_SRCS),$(MY_DEPFILE))
#
# `oops_depgen` takes values; `oops_obj_rules` takes variable names, looked up when the
# recipe runs. `-MMD`/`-MM` omit system and `-isystem` headers; `-MP` lets a deleted header
# rebuild instead of failing. Objects go under <objdir>/app, /apps (oops-apps), /oops (the
# collection) or /ext, by the source's path relative to this file's location.

ifndef OOPS_DEPS_MK
OOPS_DEPS_MK := 1

OOPS_DEPS_DIR  := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
OOPS_DEPS_APPS := $(abspath $(OOPS_DEPS_DIR)/..)
OOPS_DEPS_OOPS := $(abspath $(OOPS_DEPS_DIR)/../..)

# The makefiles without the depfiles. `-include` appends every depfile to `MAKEFILE_LIST`,
# and naming those as prerequisites would make every object depend on every depfile.
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
# One rule per source; `$(sort)` removes duplicates. The makefiles are prerequisites, so a
# flag change recompiles.
define oops_obj_rule_one
$(call oops_obj,$(1),$(4)): $(4) $$(oops_makefiles)
	@mkdir -p $$(@D)
	$$($(2)) $$($(3)) -MMD -MP -c -o $$@ $(4)
endef
oops_obj_rules = $(foreach s,$(sort $(4)),$(eval $(call oops_obj_rule_one,$(1),$(2),$(3),$(s))))

# $(call oops_ar_check,<objects>)
#
# `ar` stores a member under its basename and `ar r` replaces a member of the same name, so
# two objects sharing a file name would silently become one. This fails the build instead.
# `src/oops-deps/sdl2/oops-sdl.mk` numbers its objects for the same reason.
oops_ar_dups  = $(strip $(foreach n,$(sort $(notdir $(1))),\
                    $(if $(word 2,$(filter $(n),$(notdir $(1)))),$(n))))
oops_ar_check = $(if $(call oops_ar_dups,$(1)),$(error two objects would be archived under one \
                name, and `ar` keeps only the last: $(call oops_ar_dups,$(1)). Rename one of the \
                sources, or number the objects the way src/oops-deps/sdl2/oops-sdl.mk does))

# $(call oops_depgen,<compiler>,<compile flags>,<target>,<sources>,<depfile>)
#
# The whole-program form: one `<target>: header ...` rule per source into <depfile>. `-MT`
# names the real target; `-Qunused-arguments` because the flags include link flags; written
# through a temporary so an interrupted pass leaves no truncated depfile.
oops_depgen = @$(1) $(2) -Qunused-arguments -MM -MP -MT $(3) $(4) > $(5).tmp && mv -f $(5).tmp $(5)

endif
