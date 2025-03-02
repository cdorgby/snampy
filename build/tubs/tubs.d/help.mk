
_TUBS_LOADED_SUB_MAKES += A_help
_TUBS_ALLOWED_TOP_TARGETS += help

# The help system... or lack there of
# any tubs.d makefile that adds itself into the _TUBS_LOADED_SUB_MAKES
# variabe will have a variable <make_name>_tubs_help expended when the
# rule is ran
help:
	$(foreach _f,$(sort $(_TUBS_LOADED_SUB_MAKES)),$($(_f)_tubs_help))

# Here is example how to create a help function
# all lines must start with a TAB.
# can't have conditional directive (ifeq ()... $(if) are fine)
# must end with a blank line
define A_help_tubs_help
	@$(ECHO) " all                - Build everything"
	@$(ECHO) " <target>/all       - Build everything needed for a specific target"
	@$(ECHO) " clean              - Remove all compiled or linked output"
	@$(ECHO) " <target>/clean     - Remove all compiled or linked output for a specific target"
	@$(ECHO) " distclean          - Remove all compiled or linked output"
	@$(ECHO) " <target>/distclean - Performs a more torough clean. This generally should "
	@$(ECHO) "                      leave the source tree in the same state as if it was just"
	@$(ECHO) "                      unpacked, patched, configured, and then cleaned, keeping the patches."
	@$(ECHO) " cleandeps          - Removes dependency tracking related files."
	@$(ECHO) "                      That include files .d and .d.flags extensions."
	@$(ECHO) " install            - Copy all build outputs to staging: $(i)"
	@$(ECHO) " <target>/install   - Copy all build outputs to staging from a specific target"
	@$(ECHO) "                      If BOARD_IP environment variable is defined and the build targets a HW platform, then"
	@$(ECHO) "                      the files will also be copied (using scp) to their installation location on the target."
	@if [ ! -z "$(TUBS_TOOLCHAIN_DEP)" ]; then\
	$(ECHO) " download_toolchain - Download and install local copy of tool chain. Note: toolchain installed in /opt/toolchain"; \
	$(ECHO) "                      will be used if available"; \
	fi
	@$(ECHO)
	@$(ECHO) "Help from makefiles in tubs/tubs.d directory"
	@$(ECHO) "==============================================================="

endef