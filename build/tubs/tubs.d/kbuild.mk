
# This makefile add aditional variables that can be defined in Makefile for targets
# _config_file 
#   Name of the configuration file to use

_TUBS_LOADED_SUB_MAKES += kbuild

define ext_cmd_configure_kbuild
$(call _ext_name_cmd,$(1),pre_configure)
$(call cmd,cp,$(1),$(call _ud_dir,$(1))/.config,$(call _s_dir,$(1))/$($(1)_config_file))
$(call cmd_ext,run,$(1),oldconfig $($(1)_name),,,cd $(call _ud_dir,$(1)) && /usr/bin/make $($(1)_kbuild_make_opt) olddefconfig)
$(call _ext_name_cmd,$(1),post_configure)

endef

define ext_cmd_build_kbuild
$(call _ext_name_cmd,$(1),pre_build)
$(call cmd_ext,run,$(1),Building $($(1)_name),,,cd $(call _ud_dir,$(1)) && /usr/bin/make $($(1)_kbuild_make_opt) -j$(JOBS))
$(call _ext_name_cmd,$(1),post_build)

endef

# don't forget about DESTDIR
define ext_cmd_install_kbuild
$(call _ext_name_cmd,$(1),pre_install)
$(call cmd_ext,run,$(1),Installing $($(1)_name),,,cd $(call _ud_dir,$(1)) && /usr/bin/make DESTDIR="$(call _o_dir,$(1))" $($(1)_kbuild_make_opt) install)
$(call _ext_name_cmd,$(1),post_install)

endef

define ext_cmd_clean_kbuild
$(call cmd_ext,run,$(1),Clean in $($(1)_name),,,cd $(call _ud_dir,$(1)) && /usr/bin/make $($(1)_kbuild_make_opt) clean)
$(call _ext_name_cmd,$(1),clean)
$(call cmd,rm,$(1),$(call _stamp_built,$(1)))
$(call cmd,rm,$(1),$(call _stamp_installed,$(1)))

endef

define ext_cmd_distclean_kbuild
$(if $(ext_cmd_distclean_$($(1)_name)),$(call $(ext_cmd_distclean_$($(1)_name))))
$(call ext_cmd_clean_kbuild,$(1))
$(call cmd_ext,run,$(1),Distclean in $($(1)_name),,test -e $(call _stamp_configured,$(1)),cd $(call _ud_dir,$(1)) && /usr/bin/make $($(1)_kbuild_make_opt) distclean)
$(call _ext_name_cmd,$(1),distclean)
$(call cmd,rm,$(1),$(call _stamp_configured,$(1)))

endef

define ext_cmd_setup_kbuild
$(if $(TUBS_TOOLCHAIN_PREFIX),$(eval $(1)_kbuild_make_opt += CROSS_COMPILE=$(TUBS_TOOLCHAIN_PREFIX)))

endef

define ext_cmd_include_external_kbuild
    $$(eval $$(call copy_makefile_variable,$(1),config_file))
endef