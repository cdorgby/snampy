
_TUBS_LOADED_SUB_MAKES += make_patch
_TUBS_ALLOWED_SUBDIR_TARGETS += make_patch

# $1 = target-name
# Show target's dependencies
# The weirdness in the rule is to handle cases where _dir and _path match
# distclean existing work directory
# move unpacked dir to .mod
# remove patch files
# ask build to re-create the configured stamp file (this will unpack, patch, and configure)
# move to .orig
# move .mod back original name
# create patch
# remove .orig
define make_patch_sub_rules
$(call _o_dir_rel,$(1))/make_patch $($(1)_path)/make_patch $(if $(call sne,$($(1)_path),$($(1)_dir)),$($(1)_dir)/make_patch): .force
ifneq ($($(1)_stamped),$(TRUE))
	$(Q)$(ECHO) "Not an external target"
else
	-$$(call ext_cmd_distclean_$($(1)_ext_build),$(1))
	$$(call cmd,mv,$(1),$$(call _w_dir,$(1))/$$($(1)_unpacked_dir).mod,$$(call _w_dir,$(1))/$$($(1)_unpacked_dir))
	$$(call cmd,rm,$(1),$$(call _stamp_unpacked,$(1)))
	$$(call cmd,rm,$(1),$$(call _stamp_configured,$(1)))
	$$(call cmd,rm,$(1),$$(call _stamp_patched,$(1)))
	$$(if $$($(1)_patch_files),$$(call cmd,rm,$$($(1)_patch_files)))
	$$(call cmd,make_me,$(1),$$(call _stamp_configured,$(1)))
	-$$(call ext_cmd_distclean_$$($(1)_ext_build),$(1))
	$$(call cmd,mv,$(1),$$(call _w_dir,$(1))/$$($(1)_unpacked_dir).orig,$$(call _w_dir,$(1))/$$($(1)_unpacked_dir))
	$$(call cmd,mv,$(1),$$(call _w_dir,$(1))/$$($(1)_unpacked_dir),$$(call _w_dir,$(1))/$$($(1)_unpacked_dir).mod)
	-cd $$(call _w_dir,$(1)) && $$(call cmd,diff,$(1),$$($(1)_unpacked_dir),$$($(1)_unpacked_dir).orig,,,$$(call _w_dir,$(1))/generate_patch.diff)
	$$(call cmd,rmdir,$(1),$$(call _w_dir,$(1))/$$($(1)_unpacked_dir).orig)
	$$(call cmd,rm,$(1),$$(call _stamp_configured,$(1)))
	$$(call cmd,echo,$(1),Generated patch location: $$(call _w_dir,$(1))/generate_patch.diff)

endif
endef
#$$(call cmd,unpack,$(1),$(call _w_dir,$(1)),$$(TUBS_ARCHIVE_DIR)/$$($(1)_source_archive),,$$($(1)_unpacked_dir).orig)

define make_patch_tubs_help
	@$(ECHO) "<target>/make_patch - Generate a new patch for an external target."
	@$(ECHO) "                      This allows for easier workflow when an external package has to be modified by"
	@$(ECHO) "                      automating the creation of new patch files. You can make changes in the target's"
	@$(ECHO) "                      source directory under it's work directory, then use this target to create a new"
	@$(ECHO) "                      patch. TUBS will try to clean the source directory as much as possible. Then it"
	@$(ECHO) "                      creates a fresh copy of the target's source tree, applies existing patches and then"
	@$(ECHO) "                      compares the directory to the modified source tree."
	@$(ECHO)



endef