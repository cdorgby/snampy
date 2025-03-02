
TUBS_SYSTEM_DEPS += cmake

_cmake_build_dir = $(call _w_dir,$(1))/build
_cmake_tc_file = $(call _cmake_build_dir,$(1))/ToolchainFile.txt

define ext_cmd_configure_cmake
$(call _ext_name_cmd,$(1),pre_configure)
$(call cmd,mkdir,$(1),$(call _cmake_build_dir,$(1)),,test ! -e $(call _cmake_build_dir,$(1)))
$(call cmd_ext,run,$(1),Configuring $($(1)_name),,,cd $(call _cmake_build_dir,$(1)) && $(CMAKE) $(call _ud_dir,$(1)) $($(1)_conf_options))
$(call _ext_name_cmd,$(1),post_configure)

endef

define ext_cmd_build_cmake
$(call _ext_name_cmd,$(1),pre_build)
$(call cmd_ext,run,$(1),Building $($(1)_name),,,cd $(call _cmake_build_dir,$(1)) && /usr/bin/make -j$(JOBS) $(if $(Q),,VERBOSE=1))
$(call _ext_name_cmd,$(1),post_build)

endef

# don't forget about DESTDIR
define ext_cmd_install_cmake
$(call _ext_name_cmd,$(1),pre_install)
$(call cmd_ext,run,$(1),Installing $($(1)_name),,,cd $(call _cmake_build_dir,$(1)) && /usr/bin/make install)
$(call _ext_name_cmd,$(1),post_install)

endef

define ext_cmd_clean_cmake
$(call cmd_ext,run,$(1),Clean in $($(1)_name),,test -e $(call _stamp_configured,$(1)),cd $(call _cmake_build_dir,$(1)) && /usr/bin/make clean)
$(call _ext_name_cmd,$(1),clean)
$(call cmd,rm,$(1),$(call _stamp_built,$(1)))
$(call cmd,rm,$(1),$(call _stamp_installed,$(1)))

endef

define ext_cmd_distclean_cmake
$(if $(ext_cmd_distclean_$($(1)_name)),$(call $(ext_cmd_distclean_$($(1)_name))))
$(call ext_cmd_clean_cmake,$(1))
$(call cmd,rmdir,$(1),$(call _cmake_build_dir,$(1)))
$(call _ext_name_cmd,$(1),distclean)
$(call cmd,rm,$(1),$(call _stamp_configured,$(1)))

endef

define __setup_toolchain_file

$(call _cmake_tc_file,$(1)): | $(call _cmake_build_dir,$(1))
	$(ECHO) "set(CMAKE_SYSTEM_NAME Linux)" > $$@
	$(ECHO) "set(CMAKE_C_COMPILER $(CC))" >> $$@
	$(ECHO) "set(CMAKE_CXX_COMPILER $(CXX))" >> $$@
	$(ECHO) "set(CMAKE_FIND_ROOT_PATH $(if $(TUBS_TOOLCHAIN_SYSROOT),$(TUBS_TOOLCHAIN_SYSROOT),$(TUBS_TOOLCHAIN_PATH)))" >> $$@
	$(if $(call seq,host_i386,$(BUILD_BOARD)),$(ECHO) "set(CMAKE_C_FLAGS_INIT -m32)" >> $$@)
	$(if $(call seq,host_i386,$(BUILD_BOARD)),$(ECHO) "set(CMAKE_CXX_FLAGS_INIT -m32)" >> $$@)
	$(ECHO) "set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)" >> $$@
	$(ECHO) "set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)" >> $$@
	$(ECHO) "set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE $(if $(call seq,host_i386,$(BUILD_BOARD)),BOTH,ONLY))" >> $$@
	

$(call _cmake_build_dir,$(1)):
	$$(call cmd,mkdirp,$(1),$$(@),,,test ! -e $(@))

$(call _stamp_configured,$(1)): $(call _cmake_tc_file,$(1))
$(eval $(1)_conf_options += -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=$(call _cmake_tc_file,$(1)) -DCMAKE_INSTALL_PREFIX=$(call _o_dir,$(1)))

endef

define ext_cmd_setup_cmake
$(eval $(call __setup_toolchain_file,$(1)))
endef
