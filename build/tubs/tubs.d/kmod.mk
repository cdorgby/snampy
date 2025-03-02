
# This makefile add aditional variables that can be defined in Makefile for targets
# _config_file 
#   Name of the configuration file to use

_TUBS_LOADED_SUB_MAKES += kmod

_kmod_kernel_file = $(_o)/$(TUBS_KERNEL_IMAGE)

define ext_cmd_configure_kmod
$(call _ext_name_cmd,$(1),pre_configure)
$(call _ext_name_cmd,$(1),post_configure)

endef

define ext_cmd_build_kmod
$(call _ext_name_cmd,$(1),pre_build)
$(call cmd_ext,run,$(1),Building $($(1)_name),,,cd $(call _ud_dir,$(1)) && /usr/bin/make -C $(TUBS_KERNEL_BUILD_DIR) $($(1)_kmod_build_opt) M=$(call _ud_dir,$(1)) -j$(JOBS) modules)
$(call _ext_name_cmd,$(1),post_build)

endef

#$(call cmd_ext,run,$(1),Installing $($(1)_name),,,cd $(call _ud_dir,$(1)) && /usr/bin/make DESTDIR="$(call _o_dir,$(1))" $($(1)_kmod_build_opt) instal-l)
# don't forget about DESTDIR
define ext_cmd_install_kmod
$(call _ext_name_cmd,$(1),pre_install)
$(call cmd_ext,run,$(1),Installing $($(1)_name),,,cd $(call _ud_dir,$(1)) && /usr/bin/make INSTALL_MOD_PATH="$(call _o_dir,$(1))" -C $(TUBS_KERNEL_BUILD_DIR) $($(1)_kmod_build_opt) M=$(call _ud_dir,$(1)) -j$(JOBS) modules_install)
$(call _ext_name_cmd,$(1),post_install)

endef

define ext_cmd_clean_kmod
$(call cmd_ext,run,$(1),Clean in $($(1)_name),,,cd $(call _ud_dir,$(1)) && /usr/bin/make -C $(TUBS_KERNEL_BUILD_DIR) $($(1)_kmod_build_opt) M=$(call _ud_dir,$(1)) -j$(JOBS) clean)
$(call _ext_name_cmd,$(1),clean)
$(call cmd,rm,$(1),$(call _stamp_built,$(1)))
$(call cmd,rm,$(1),$(call _stamp_installed,$(1)))

endef

#$(call cmd_ext,run,$(1),Distclean in $($(1)_name),,test -e $(call _stamp_configured,$(1)),cd $(call _ud_dir,$(1)) && /usr/bin/make $($(1)_kmod_build_opt) distclea-n)
define ext_cmd_distclean_kmod
$(if $(ext_cmd_distclean_$($(1)_name)),$(call $(ext_cmd_distclean_$($(1)_name))))
$(call ext_cmd_clean_kbuild,$(1))
$(call _ext_name_cmd,$(1),distclean)
$(call cmd,rm,$(1),$(call _stamp_configured,$(1)))

endef

define ext_cmd_setup_kmod
$$(if $(TUBS_TOOLCHAIN_PREFIX),$$(eval $(1)_kmod_build_opt = ARCH=arm CROSS_COMPILE=$(TUBS_TOOLCHAIN_PREFIX)-))

$$(eval $$(call _stamp_built,$(1)): $(_kmod_kernel_file) $$(wildcard $$(call _s_dir,$(1))/$$($(1)_source_dir)/*.c $$(call _s_dir,$(1))/$$($(1)_source_dir)/*.h))
$$(eval $$(call _ur_dir,$(1)): $$($(1)_source_dir)/Makefile)

endef

define ext_cmd_include_external_kmod
endef