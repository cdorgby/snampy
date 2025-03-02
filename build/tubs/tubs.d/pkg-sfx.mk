# Creates self-extracting packages
# One package consists of everything copied in to an install_root

# To get started define pkgsfx variable with names of packages in it.
# These can be defined in any Makefile
# Then each defined package becomes a target-name
#
# Package's contents can be specified two way. First implicitly through
# install_root variables, or second by using _targets variable.
#
# If the target's _install_root matches packages _install_root, all outputs
# produced by a target will be included into the package.
#
# If there is a <pkg>_targets variable defined the outputs produced by the
# targets listed in the variables will be included into the package.
#
# The following variabes can be defines for each package
# _version
#   Version string to embed into the package, displayed with --version
# _install_root
#   The install_root path associated with this package. Any target that
#   is built by TUBS can define _install_root variable. All target that
#   match an _instal_root with a package will be included.
#   One package for the whole build system can be defined without an
#   _install_root variable. This will be treated as the "default" package
#   and any built target that doesn't specify an install_root will be 
#   included in this default package.
# _target
#  Specifies a list of target names who's outputs will be included into the package
#
# _instrall_root and _target are not mutually exclusive.
# If a target defines an _install_root that matches one from a package, the 
# build outputs from each target that maches the _install_root will be included
# into the package.
# If a target doesn't define an _install_root variable and there is a packages
# that is considered to be defuaul, the target's outputs will be included into
# the defaul package.
# If _install_root doesn't match any packages, then the target will only be
# included into packages when it is explicitly listed in the package's _targets
# variables


_TUBS_LOADED_SUB_MAKES += pkgsfx
_TUBS_ALLOWED_TOP_TARGETS += packages package_list
_TUBS_ALLOWED_SUBDIR_TARGETS += files rebuild remove

_default_pkg :=
_pkgsfx_all_pkgs :=
_from_sfx = $(patsubst _%,%,$(patsubst pkgsfx_%,%,$(1)))
_to_sfx = $(call _path_to_name,$(1),pkgsfx_)
_pkgsfx_to_pkg_file = $(call _o_dir,all)/$(call _from_sfx,$(1)).pkg

# $1 = target-name
# $2 = output-file
# $3 = install-root
# $4 = file list
# $5 = version
# $6 = package name and rest of the arguments
#      unpack prefix
cmd_create_sfx          = $(if $(1),$(FAKEROOT) $(T)/scripts/create_sfx.sh $(3) $(4) $(5) $(6) $(2),sfx)

# $1 = target-name
# $2 = to file to output file list into
# $3 = path to pkg
cmd_create_manifest     = $(if $(1),sh $(3) --list > $(2),manifest)

# $1 install-root-name
get_installed_outputs = $(foreach _t,$($(1)),$(call _get_install_files,$(_t)))

# #1 = target-name
# $2 = package name
# check to make sure that all targets mentioned in the package exists
define _check_pkg_targets
$(foreach _t,$($(2)_targets),$(if $(call find_target,$(1),$(_t),$(2)),,))
endef

# $1 = Name of target in the Makefile
# Parse and setup package related info
# This is called from include_subdir when ever pkgsfx = ... is seen in a makefile
define pkgsfx_tubs_include_custom
# complain if _default_pkg is already set and another pkg doesn't have the install_root defined
$$(if $(_default_pkg),\
	$$(if $$($(1)_install_root),,\
		$(if $$($(1)_targets),,\
			$$(call _tubs_error,$(_target),Default pkg is already defined to '$$(_default_pkg)'$$(comma) another pkg defintion '$(1)' in 'pkgsfx' doesn't define an install root in '$(1)_install_root'. Only one package can be defined without an install_root.))))

# set _default_pkg to the first pkg defined without an install_root, also set _install_root for that package to root
$$(if $$($(1)_targets),\
	$$(eval $$(call _to_sfx,$(1))_targets := $$($(1)_targets)),\
	$$(if $$($(1)_install_root),\
		$$(eval $$(call _to_sfx,$(1))_install_root := $(call _path_to_ir,$($(1)_install_root))),\
		$$(eval _default_pkg := $(1))$$(eval $$(call _to_sfx,$(1))_install_root := install_root)))

$$(eval _pkgsfx_all_pkgs += $$(call _to_sfx,$(1)))
$$(eval $(call _to_sfx,$(1))_version := $$(if $($(1)_version),$($(1)_version),0.0.0))
$$(eval $(call _to_sfx,$(1))_unpack_prefix := $(if $($(1)_unpack_prefix),$($(1)_unpack_prefix),""))
$$(eval $(call _to_sfx,$(1))_target := $(_target))

endef

# little hack to insert new-line chracter into variables, must contain two blank lines
# Since sed is used on the file_list anyways, why not just add new-lines using that?
# because this operation doesn't run often enough to try to to optimize that much.
# but file paths can have spaces in them, this was path of least resistence
define NL


endef

define _pkgsfx_rules_empty
	$(call cmd,echo,all,Skipping $$(patsubst $(S)%,%,$$@)$(comma) nothing built that installs into the '$(call _from_sfx,$(1))_install_root' installation root)
endef

define _pkgsfx_rules_make_pkg
	$$(file >$$@.file_list,$$(foreach _f,$$(patsubst $$(call _ir_to_dir,$$($(1)_install_root))/%,%,$$(filter-out %.tubs_flags,$$^)),$$(_f)$$(NL)))
	$$(call cmd,sed,all,$$@.file_list,-i -e 's/^ *//g')
	$$(call cmd,rm,all,$$@,,,test -e $$@)
	$$(call cmd,create_sfx,all,$$@,$$(call _ir_to_dir,$$($(1)_install_root)),,,$$@.file_list,$$($(1)_version),$$(strip $$(call _from_sfx,$(1)) $$($(1)_unpack_prefix)))
	$$(call cmd,rm,all,$$@.file_list,,,test -e $$@)
	$$(call cmd,create_manifest,all,$$@.manifest,$$@)
endef

define show_direct_pkg
$(1)/files:
	@$(ECHO) $(1):
	$(foreach _f,$(strip $(foreach ___t,\
	         $(foreach __t, \
						$($(1)_targets),\
						$(call find_target,$($(1)_target),$(__t)) $(call for_all_targets_libraries_deps,$(call find_target,$($(1)_target),$(__t)))),\
			 $(call _get_install_files,$(___t),_i_dir))),@$(ECHO) $(_f)\ 
	)
endef

define show_install_root_pkg

$(1)/files:
	@$(ECHO) "($(1)_install_root)"

endef

# 1. Add the generated file to to be cleaned when ever clean at top-level runs
# 2. associate pkg file with the install_root location, and the recipe to create the package
#    Add a message telling people why we are not creating an empyt package
# 3. make all package files dependencies of packages target,this kicks of
#    the whole build sequence if needed
define _pkgsfx_rules

$(if $($(1)_targets),\
	$(eval $(call _check_pkg_targets,$($(1)_target),$(1)))\
	$(eval __install_files := $(strip $(foreach ___t,\
	           $(foreach __t, \
	                      $($(1)_targets),\
	                      $(call find_target,$($(1)_target),$(__t)) $(call for_all_targets_libraries_deps,$(call find_target,$($(1)_target),$(__t)))),\
	           $(call _get_install_files,$(___t),_i_dir)))))

$(eval all_clean_files += $(call _pkgsfx_to_pkg_file,$(1)) $(call _pkgsfx_to_pkg_file,$(1)).file_list $(call _pkgsfx_to_pkg_file,$(1)).flags)

$(call _pkgsfx_to_pkg_file,$(1)): $(call _pkgsfx_to_pkg_file,$(1)).tubs_flags $(call get_installed_outputs,$($(1)_install_root)) $(if $($(1)_targets),$(__install_files))
	$(if $(call get_installed_outputs,$($(1)_install_root)) $(if $($(1)_targets),$(__install_files)),\
	     $(call _pkgsfx_rules_make_pkg,$(1)),\
		 $(call _pkgsfx_rules_empty,$(1)))

$(call _pkgsfx_to_pkg_file,$(1)).tubs_flags: .force | $(patsubst %/,%,$(dir $(call _pkgsfx_to_pkg_file,$(1))))
	$$(call _check_and_store_flags,$(1),$$@,$$($(1)_version)$$($(1)_install_root)$$($(1)_user)$$($(1)_group)$$($(1)_perm_mode))

$(if $($(1)_targets),$(call show_direct_pkg,$(1)),$(call show_install_root_pkg,$(1)))

$(1)/rebuild: $(call _pkgsfx_to_pkg_file,$(1))

$(1)/remove: .force
	$$(call cmd,rm,all,$$(call _pkgsfx_to_pkg_file,$(1)),,,test -e $$(call _pkgsfx_to_pkg_file,$(1)))

packages: $(call _pkgsfx_to_pkg_file,$(1))
endef

# XXX: WARNING! there must be a space after the '\' at the end of the $(ECHO) line
define _show_pkg_list
	$(foreach _p,$(_pkgsfx_all_pkgs),\
	@$(ECHO) "pkg name: $(_p) path: $(call _pkgsfx_to_pkg_file,$(_p))"\ 
	)
endef

# The sole reason for this function is to add the build done, output
define pkgsfx_rules
$(call _o_dir,all)/.dockerignore:
	$(Q)$(ECHO) "*" > $$@
	$(Q)$(ECHO) "!*.pkg" >> $$@

$(foreach _p,$(_pkgsfx_all_pkgs),$(eval $(call _pkgsfx_rules,$(_p))))
packages: | $(call _o_dir,all)/.dockerignore
	$$(call echo_build_done,all)

package_list:
	$$(call _show_pkg_list)
endef

define pkgsfx_tubs_setup_custom
$(eval $(pkgsfx_rules))
endef

define pkgsfx_tubs_load_tail
$(if $(_default_pkg),,$(call _tubs_error,all,There is no default pkg defined. Make sure that one of the defined packages doesn't specify an install_root))
$(foreach _p,$(_pkgsfx_all_pkgs),,$(if $($($(_p)_install_root)),,SFX Package '$(call _from_sfx,$(_p))' refers to an install-root '$(call _ir_to_path,$($(_p)_install_root))' that doesn't have any outputs))

endef

define pkgsfx_tubs_help
	@$(ECHO) " packages           - Create packages in $(o)"
	@$(ECHO) " package_list       - List of packages to create"
	@$(ECHO) "                      Must be ran from from top of tree only"
	@$(ECHO) " <pkg_name>/files   - List of files that will be part of the package"
	@$(ECHO) " <pkg_name>/rebuild - Build the pckage, if it doesn't exist"
	@$(ECHO) " <pkg_name>/remove  - Remove the package file from the output directory"
	@$(ECHO)

endef