
_TUBS_LOADED_SUB_MAKES += show_deps
_TUBS_ALLOWED_SUBDIR_TARGETS += show_deps

# this is recusion depth, each time __print_target_deps gets called add an 'x' 
# to __ptd, once __ptd and __ptd_depth match we've hit our depth limit
__ptd_depth = x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x
define __print_target_deps
$(if $(call seq,$(__ptd),$(__ptd_depth)),$(warning $(__ptd_chain) toooo deeeeep),$(eval __ptd += x)$(eval __ptd_chain += $($(2)_path)))
$(if $(call seq,$(__ptd),$(__ptd_depth)),,$(Q)$(ECHO) "$(4)$($(2)_path)$(if $(call _rd_get,$(1),$(2)), ($(call _rd_get,$(1),$(2))))")
$(if $(call seq,$(__ptd),$(__ptd_depth)),,$(foreach __tb,$($(2)_tubs_$(3)),$(call __print_target_deps,$(1),$(__tb),$(3),$(4)$(space))))
endef

define _print_target_deps
$(if $($(2)_tubs_$(3)),$(call __print_target_deps,$(1),$(2),$(3)))
$(foreach _tb,$($(2)_targets),\
$(eval __ptd := )\
$(eval __ptd_chain := )\
$(call _print_target_deps,$(_tb),$(_tb),$(3))\
$(if $(call seq,$(__ptd),$(__ptd_depth)),$(warning $($(1)_path):$(__ptd_chain) toooo deeeeep)))
endef

define print_target_deps
$(call _print_target_deps,$(1),$(1),$(2))
endef

# $1 = target-name
# Show target's dependencies
# The weirdness in the rule is to handle cases where _dir and _path match
define show_deps_sub_rules
$(call _o_dir_rel,$(1))/show_deps $($(1)_path)/show_deps $(if $(call sne,$($(1)_path),$($(1)_dir)),$($(1)_dir)/show_deps): .force
	$(Q)$(ECHO) "Includes:"
	$$(call print_target_deps,$(1),includes)
	$(Q)$(ECHO) "Libraries:"
	$$(call print_target_deps,$(1),libraries)
endef

define show_deps_tubs_help
	@$(ECHO) " show_deps          - Print target's dependencies, this is can be recursive"

endef
#$(call for_all_targets_include_deps,$(1))