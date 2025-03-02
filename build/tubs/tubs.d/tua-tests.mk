

# Add support for TAU test framework and running the tests

# Makefile variable:
#     tua-tests       : takes a list of source code files, each file is a TAU test.
#
# The files names in the tua-test are converted to target names by replacing each '/' with an '_'
# and strip off the extension. src/test/some_test.c -> src_test_som_test
#
# Target variables:
#     <target>_librarie       : list of libraries that the test needs to run
#     <target>_includes       : list of header only dependencies or addition -I to add to test
#

_TUBS_LOADED_SUB_MAKES += tua-tests
_TUBS_ALLOWED_TOP_TARGETS += run_tests list_tests rebuild_tests

_tua_tests_list :=

#_tua_test_name = test-$($(_target)_name)-$(basename $(notdir $(1)))
_tua_test_name = $(basename $(notdir $(1)))
_tua_test_target = $(call _path_to_name,$(basename $(1)),$(_target)_)
_source_to_tua_test = $(foreach _so,$(1),$(call _o_dir,$(call _tua_test_target,$(1)))/$(call _tua_test_name,$(_so)))
_source_to_tua_object_full = $(foreach _so,$(1),$(call _o_dir,$(call _tua_test_target,$(1)))/$(call _tua_test_name,$(_so)).o)
_source_to_tua_object = $(foreach _so,$(1),$(call _tua_test_name,$(_so)).o)

_tua_makefile_var = $(call _path_to_name,$(basename $(1)))
_tua_output_dir = $(patsubst %/,%,$(call _o_dir,$(call _tua_test_target,$(1)))/$(dir $(1)))

# $1 = target-name
# $2 = Path to test executable
# based on $(1)_link_type expand appropriate cmd_link_ variable
cmd_run_tua = $(__cmd_tr3)$(if $(1),$(2) --output=./test-$(BUILD_BOARD)-xunit.xml;,tua-test)

# the extra space at the end on the second line of the foreach is important
define _show_test_list
	$(foreach _p,$(_tua_tests_list),\
	@$(ECHO) "test : $(call _tua_test_name,$(_p)) path: $(_p)"\ 
	)
endef

ifeq ($(TUBS_CONFIG_HW_TARGET),HOST)
define tua-tests_tubs_setup_custom
.PHONY: run_tests
run_tests: $(_tua_tests_list)
	$$(foreach _test,$$(_tua_tests_list),$$(_test);)

rebuild_tests: $(_tua_tests_list)

list_tests:
	$$(call _show_test_list)


$(addprefix $(o),$(_tua_tests_dirs)):
	$$(call cmd,mkdirp,$(1),$$(@),,,test ! -e $(@))

$(_amalgamation_name):

endef
else

define tua-tests_tubs_setup_custom
run_tests rebuild_tests test_list:
	@echo "Only available on host"

endef

endif

define _add_test

$(eval $(call _tua_test_target,$(1))_path := $($(_target)_path))
$(eval $(call _tua_test_target,$(1))_dir := $($(_target)_dir)/$(dir $(1)))
$(eval $(call _tua_test_target,$(1))_sources := $(notdir $(1)))
$(eval $(call _tua_test_target,$(1))_name := $(call _path_to_name,$(dir $(1))))
$(eval $(call _tua_test_target,$(1))_link_type := $(if $(filter %.cpp,$(suffix $($(call _tua_test_target,$(1))_sources))),app_cpp,app_c))
$(eval $(_target)_clean_files := $(strip $($(_target)_clean_files) $(call _get_objects,$(call _tua_test_target,$(1)))))

$(eval _tua_tests_targets := $(strip $(_tua_tests_targets) $(call _tua_test_target,$(1))))
$(eval _tua_tests_list := $(strip $(_tua_tests_list) $(call _source_to_tua_test,$(1))))
$(eval _tua_tests_dirs := $(sort $(_tua_tests_dirs) $(patsubst %/,%,$($(_target)_path)/$(dir $(1)))))

$(call _tubs_log,$(call _target_to_makefile,$(_target)): Adding test '$(call _tua_test_name,$(1))')

$(if $($(call _tua_makefile_var,$(1))_libraries),$(eval $(call _tua_test_target,$(1))_libraries := $($(call _tua_makefile_var,$(1))_libraries)))
$(if $($(call _tua_makefile_var,$(1))_includes),$(eval $(call _tua_test_target,$(1))_includes := $($(call _tua_makefile_var,$(1))_includes)))

$(eval $(call resolve_libraries,$(call _tua_test_target,$(1))))
$(eval $(call resolve_includes,$(call _tua_test_target,$(1))))

$(call _setup_object,$(call _tua_test_target,$(1)),$(notdir $(1)),$(call _source_to_object,$(call _tua_test_target,$(1)),$(notdir $(1))))

$(call _source_to_tua_test,$(1)): $(call _get_objects,$(call _tua_test_target,$(1))) $(call get_target_deps_as_libs,$(call _tua_test_target,$(1)),_o_dir) | $(call _o_dir,$(call _tua_test_target,$(1))) $(TUBS_TOOLCHAIN_DEP)
	$$(call cmd,link,$(call _tua_test_target,$(1)),$(call _source_to_tua_test,$(1)),,,$$(call _get_objects,$(call _tua_test_target,$(1))))

endef

define tua-tests_tubs_include_custom
$(foreach _s,$(1),$(eval $(call _add_test,$(_s))))
endef

define tua-tests_tubs_help
	@echo " run_tests          - Run all tests."
	@echo " list_tests         - List all tests."
	@echo

endef