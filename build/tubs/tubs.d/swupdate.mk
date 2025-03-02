# Create swupdate update bundle based on the current build
# the hard work for making the update bundle is done in an external
# script, here we just create dependency on the kernel and .pkg
# file that are defined using pkgsfx
_TUBS_LOADED_SUB_MAKES += swupdate
_TUBS_ALLOWED_TOP_TARGETS += swupdate

UPDATE_BUNDLE_VERSION ?= 0.0.0
UPDATE_KERNEL_VERSION ?= 0.0.0
# $1 = target-name
# $2 = output
# $3 = location of images (output directory)
# $4 = key-type
# $5 = version
# $6 = kernel version
# $7 = kernel file name
cmd_swupdate    = $(__cmd_tr6)$(if $(1),$(T)/scripts/create_swupdate.sh $(3) $(4) $(5) $(6) $(BUILD_BOARD) $(PROJECT_NAME) $(2) $(2).pkg_list)

_fixed_package_list = $(strip $(addprefix pkgsfx_,$(filter-out pkgsfx_%,$(1))) $(filter pkgsfx_%,$(1)))

#_bundle_path = $(o)$(TARGET)-update-bundle.swu
_bundle_paths =
_bundle_names =

_bundle_packages = $(foreach __bp,$($(1)_swupdate),$(call _to_sfx,$(__bp)))
_bundle_package_paths = $(strip $(foreach _bpp,$(call _bundle_packages,$(1)),$(call _pkgsfx_to_pkg_file,$(_bpp))))

define swupdate_tubs_include_custom
$(call _tubs_log,$(call _target_to_makefile,$(_target)): Adding update bundle to build '$(1)')
$(eval _bundle_path := $(strip $(_bundle_path) $(o)$(1)-update-bundle.swu))
$(eval _bundle_names := $(strip $(_bundle_names) $(o)$(1)-update-bundle.swu))
endef

define _bundle_rule
$(1): $(1).flags $(1).pkg_list $(o)version_info $(call _bundle_package_paths,$(patsubst $(o)%-update-bundle.swu,%,$(1))) $$(if $$(TUBS_KERNEL_IMAGE),$(o)$$(TUBS_KERNEL_IMAGE))
	$$(call cmd,swupdate,all,$$(@),,,$(patsubst %/,%,$(o)),test,$(UPDATE_BUNDLE_VERSION),$(UPDATE_KERNEL_VERSION))
	$$(if $(BOARD_IP),$(SCP) $(if $(TUBS_BOARD_SSH_KEY),-i $(TUBS_BOARD_SSH_KEY) )$$@ root@$(BOARD_IP):/tmp)
	$$(if $(BOARD_IP),$(SSH) $(if $(TUBS_BOARD_SSH_KEY),-i $(TUBS_BOARD_SSH_KEY) )root@$(BOARD_IP) 'chmod o+r /tmp/$$(notdir $$@)')
	$$(if $(BOARD_IP),$(SSH) $(if $(TUBS_BOARD_SSH_KEY),-i $(TUBS_BOARD_SSH_KEY) )root@$(BOARD_IP) 'swupdate-client --force --exit-when-done /tmp/$$(notdir $$@)')
	$$(call cmd,rm,all,$$(@).pkg_list)

endef

define swupdate_tubs_setup_custom
swupdate: $(_bundle_path)
	$$(call echo_build_done,all)

$(addsuffix .flags,$(_bundle_path)): .force | $(o)
	$$(call _check_and_store_flags,$(1),$$@,$$(call _bundle_packages,$$(patsubst $(o)%-update-bundle.swu.flags,%,$$@))$(UPDATE_BUNDLE_VERSION)$(UPDATE_KERNEL_VERSION)$(BOARD_IP))

$(addsuffix .pkg_list,$(_bundle_path)): | $(o)
	$(Q)$(call cmd,echo,$(1),Updating $$@)$$(file >$$@,$$(strip $$(call _bundle_package_paths,$$(patsubst $(o)%-update-bundle.swu.pkg_list,%,$$@)) $(o)version_info))

.SECONDARY: $(addsuffix .pkg_list,$(_bundle_path))

$(eval $(foreach _bp,$(_bundle_names),$(call _bundle_rule,$(_bp))))


.PHONY: $(o)version_info

$(o)version_info:
	$(Q)$(call cmd,echo,$(1),Updating $$@)$$(file >$$@,version=$(UPDATE_BUNDLE_VERSION))
	$(Q)$$(file >>$$@,dev_version=$(GIT_DESCRIBE))
	$(Q)$$(file >>$$@,os_version=$(UPDATE_KERNEL_VERSION))
	$(Q)$$(file >>$$@,project_name=$(PROJECT_NAME))
	$(Q)$(foreach __t,$(_TUBS_VERSIONS),$$(file >>$$@,$(__t)))

all/clean: swupdate/clean

swupdate/clean: .force
	$$(call cmd,rm,all,$(_bundle_path))
endef

define swupdate_tubs_help
	@$(ECHO) " swupdate/clean     - Remove software update bundle"
	@$(ECHO) " swupdate           - Create software update bundle"
	@$(ECHO) "                      If BOARD_IP environment variable is set to an IP address"
	@$(ECHO) "                      swupdate will scp the update to the target and run swupdate-client."
	@$(ECHO) "                      Bundle Path: $(_bundle_path)"
	@$(ECHO)

endef

# add bundle to all clean_files
define swupdate_tubs_load_tail
$(eval all_clean_files += $(o)$(TARGET)-update-bundle.swu)
endef