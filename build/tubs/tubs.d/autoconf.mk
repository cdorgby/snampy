

define ext_cmd_configure_autoconf
$(call _ext_name_cmd,$(1),pre_configure)
$(call cmd_ext,run,$(1),Autogen $($(1)_name),,! test -f $(call _ud_dir,$(1))/configure,cd $(call _ud_dir,$(1)) && ./autogen.sh $($(1)_autogen_options))
$(call cmd_ext,run,$(1),Configuring $($(1)_name),,,cd $(call _ud_dir,$(1)) && ./configure $($(1)_conf_options))
$(call _ext_name_cmd,$(1),post_configure)

endef

define ext_cmd_build_autoconf
$(call _ext_name_cmd,$(1),pre_build)
$(call cmd_ext,run,$(1),Building $($(1)_name),,,cd $(call _ud_dir,$(1)) && /usr/bin/make -j$(JOBS))
$(call _ext_name_cmd,$(1),post_build)

endef

# don't forget about DESTDIR
define ext_cmd_install_autoconf
$(call _ext_name_cmd,$(1),pre_install)
$(call cmd_ext,run,$(1),Installing $($(1)_name),,,cd $(call _ud_dir,$(1)) && /usr/bin/make $(if $($(1)_use_destdir),DESTDIR="$(call _o_dir,$(1))" ) install)
$(call _ext_name_cmd,$(1),post_install)

endef

define ext_cmd_clean_autoconf
$(call cmd_ext,run,$(1),Clean in $($(1)_name),,test -e $(call _stamp_configured,$(1)),cd $(call _ud_dir,$(1)) && /usr/bin/make clean)
$(call _ext_name_cmd,$(1),clean)
$(call cmd,rm,$(1),$(call _stamp_built,$(1)))
$(call cmd,rm,$(1),$(call _stamp_installed,$(1)))

endef

define ext_cmd_distclean_autoconf
$(if $(ext_cmd_distclean_$($(1)_name)),$(call $(ext_cmd_distclean_$($(1)_name))))
$(call ext_cmd_clean_autoconf,$(1))
$(call cmd_ext,run,$(1),Distclean in $($(1)_name),,test -e $(call _stamp_configured,$(1)),cd $(call _ud_dir,$(1)) && /usr/bin/make distclean)
$(call cmd_ext,run,$(1),Autogen.sh --clean in $($(1)_name),,test -e $(call _stamp_configured,$(1)),cd $(call _ud_dir,$(1)) && ./autogen.sh --clean)
$(call _ext_name_cmd,$(1),distclean)
$(call cmd,rm,$(1),$(call _stamp_configured,$(1)))

endef

define ext_cmd_setup_autoconf
$(eval $(1)_conf_options += --prefix="$(patsubst %//,/%,$(if $($(1)_use_destdir),$($(1)_install_root)/,$(call _o_dir,$(1))))")
$(if $(TUBS_TOOLCHAIN_PREFIX),$(eval $(1)_conf_options += --host=$(TUBS_TOOLCHAIN_PREFIX) --target=$(TUBS_TOOLCHAIN_PREFIX) --build=x86_64-linux-gnu))

endef
