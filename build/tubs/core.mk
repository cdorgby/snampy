################################################################
# Original code comes from TUBS by dorgby.net
# BSD 2-Clause License
# Copyright (c) dorgby.net 2013
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# 1. Redistributions of source code must retain the above copyright notice, this
# list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
# this list of conditions and the following disclaimer in the documentation
# and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


####################### Warnings ################################
# 1. If you decide to edit this file make sure that your tab key and
# indentention produces spaces not tab.
#   Tabs should only be used at start of recipes
# 2. Be careful of the names for variables that you use, there are
# no concepts of namespaces in make so
#   $(foreach _o,1 2 3 4,$(foreach _o,5 6 7 8,$(_o)),$(_o))
#   will probably leave you with a few surprises
# avoid using _a, _d, _l, _t, in include_subdir and friends code
#  (unless of course you know what you are doing)
# 3. Be careful of spaces, do what ever you can to make sure your code
# doesn't insert extra spaces into the output. $(strip) is cheap
####################### Warnings ################################
# You've been warned
################################################################


# grab current time
_TUBS_BUILD_START_TIME := $(shell date +%s)

#
###############################################################################
# Generated Target Variables
#
# all of these exist with the target-name prepended to the the name. So there
# will be many instances of these variables, but all will be unique because of 
# the target-name prefix.
# 
# For functions that expect target-name as their argument the usual way
# of accessing the variables is $($(1)_dir). If the variable is being used in
# an $(eval ) the second expansion should have double dollar signs to 
# make sure that expension happens during a $(eval ). eg. $$($(1)_dir),
# note that only the first set is quoted.
#
###############################################################################
# _dir
#   Contains the directory where the target's makefile resides. This is 
#   can be different from _path
# _parent
#   target-name of the parent
# _path
#   Path to the target (ie. it's path relative to top). This cna be different
#   from _dir as _path refers to the target while _dir refers to the directory
#   that contains the target's source/Makefile
# _targets
#   Contains a space seperated list of child-targets
# _outputs
#   Contains a space seperated list of outputs that will be produces by
#   the target
# _sources
#   Space sperated list of source files for this target. Meaningful for 
#   targets that are built by TUBS itself. (ie. apps & libraries)
# _link_type
#   For targets that are built by TUBS, this determines how the output for
#   the target is going to be linked. This should be suffix to one of the 
#   'cmd_link_xxx' commands.
# _clean_files
#   List of files to remove during 'make clean'
# _clean_dirs
#   list of directories in the target-name's output directory to remove
#   during clean step
# _post_makefile
#   Name of additional Makefile to include after all Makefile have been
#   processed.
# _post_clean_rules
#   Additional rules for clean targets. These rules will be added 
#	as dependencies of $(target-name)_path/clean and friends targets.
#	The rules defined here needs to be defined in the Makefile or
#	one of the post-makefiles
# _extra_files
#   List of additional files to copy during build/install steps.
#   Names should be relative to the $(target-name)_dir. If the paths contain
#   dir components 
# _extra_dirs
#   List of additional directories to create in both the output and install
#   directories
# _extra_output_dirs
#   List of additional directories to create in the output directory. The contents
#   of this variables comes from detecting dir components in the source files
#   paths used to build the target
# _source_archive
#   Name of the archive that contains the target's source code.
# _source_dir
#   Name of the directory contain the source code. Relative to $($(target)_path).
# _source_skip
#   Don't createh source directory for target in it's work directory. Useful if
#   there is a need to build in place.
# _unpacked_dir
#   Name of the directory that will be created when the target's source archive is
#   unpacked. This requires _source_archive to be set for the target.
#
# _work_dir
#   The working directory for the target. Location where various .stamp files
#   and, if needed, the target's source code is unpacked.
# _version
#   Version stringa... tbd
# _install_root
#   Installation location (install prefix in autoconf terms). This is the
#   location where the application expects to to live on a live system.
#   The bin, lib and other such directories are located relatives to target's
#   install root location. So for targets with an empty install root, thier
#   bin directory will be '/bin'.
# _local_includes
#   List of extra directories relatives to target-name's source to add as
#  -I directives when compiling files belonging to the target
# _install_as
#   install path of the target, only useful for apps and libraries built by TUBS.
#   if not specified bin and lib will be used. This variable is autogenerated from
#   the target name
# _perm_mode
#   Unix permissions to set on outputs for a target. Should be the 'MODE' part of
#   the command line arguments to chmod
# _user
#   User name/id to set use owner on the file
# _group
#   Group name/id to set group owner on the file
#
# _ext_build
#   type of external build system used. The value of this variable is used
#   to figure which ext_cmd_xxx_<target> command to call. By default it will
#   set to target's short name. So assuming your target has a unique name across
#   all of the build system, when the recipes for the stamps are invoked these
#   functions will be $(call ) as recipe lines.
#   If the $(target-name)_ext_build was set to custom name then ext_cmd_xxx_<that name>
#   functions will be expanded. If they are available they will be expanded instead.
#   For example the autoconf.mk will defined ext_cmd_install_autoconf and other
#   functions. That's how automagic build integration is implemented.
#
# _patch_dirs
#   List of directories containing the patches for the target.
# _patch_files
#   List of actual patch files, auto-generated from _patch_dirs
# _use_destdir
#   For things like autoconf use DESTDIR to install packages, while setting
#   the prefix to the install_root.
# _conf_options 
#   additional options that are passed to the external build system configuration
# _cmd_env
#   additional environment variables to set when ext_cmd is invoked for a specific
#   target
#
# _libraries
#   target's libraries. Including -l -L and paths to other targets
# _includes
#   target's includes. Including -I and paths to other targets
# _tubs_includes
#   list of target-names of that a target depends on.
#   These items are extracted from $(target-name)_includes
# _tubs_libraries
#   list of target-names of that a target depends on.
#   These items are extracted from $(target-name)_libraries
# _export_includes
#   list of additional include paths that the targets that depends
#   on a target that defines this variable, will get.
#   Currently assuming that these directories are relative to target's
#   output directory
#
# _stamped
#   Will be set to $(TRUE) for any target that uses stamp files instead of
#   or in addition to actual output files.

# _build_env
#   Allow variables to be passed directly to the /usr/bin/env invocation
#
# _cflags
#   These flags will be used during the compilation off all of .c source files
#   that directly belong to the target
# _cxxflags
#   Ditto for C++ compiler flags
# _ldflags
#   Linker flags
# _cppflags
#   Additional -I arguments to add (should include the -I)
# _defines
#  Additional -D to add, (should include the -D and the value should be quoted)
#
###############################################################################
# Variables that can be defined in Makefiles
###############################################################################
# Each Makefile must define at least one of the following variables: 
#    'apps', 'libs', 'externals', 'subdirs', or 'empty'
#
# Each variable can contain a list of target names that will be built, or in case
# 'subdirs' a list of sub-directories to process. 'empty' can be used to create a
# place holder Makefile that doesn't do anything. TUBS will complain about empty
# Makefiles otherwise.
#
# Any target's listed in the 'apps', 'libs, or 'externals' will be built as application,
# shared-libraries or an external project (integration with external build systems).
# Each target listed should be unique inside of the current Makefile. *HOWEVER* the
# variable namespace (such as it exists in GNU Make) is not cleared between each
# Makefile as each Makefile is not launches int's own process.
#
# Once a target is know, it can have it's own variables defined that will instruct TUBS
# on how to build the actual target. Most of these variables are copied directly to the
# generated variables defined above, some (e.g. _source_glob) go through some processing
# For example:
#
#     apps = my_fancy_app
#     my_fancy_app_sources = main.c
#     my_fancy_app_cflags = -Wall
#
#
# list of variables meaningful for 'apps' and 'libs':
# _sources_glob
# _sources
# _libraries
# _includes
# _install_root
# _extra_files
# _extra_dirs
# _cflags
# _cxxflags
# _ldflags
# _cppflags
# _defines
#
# Can be used for all target types:
# _extra_clean_dirs
# _extra_clean_files
# _export_includes
#
# meaningful for externals:
# _patch_files
# _patch_dirs
# _work_dir
# _ext_build
# _unpacked_dir
#
# _url
# _hash
# _hash_type
# _source_dir
# _source_archive
# _source_skip
# _use_destdir
# _conf_options
# _local_includes
# _cmd_env
# _perm_mode
# _user
# _group

# figure out the location of the build files
ifeq ($(T),)
T := $(realpath $(dir $(lastword $(MAKEFILE_LIST))))
endif
_ME ::= $(realpath $(lastword $(MAKEFILE_LIST)))

ifeq ($(S),)
S := $(shell pwd)/
endif

# make sure that s ends with a slash, don't use $(S) directly because it could be
# over-written from command line and we don't want that
s := $(abspath $(S))/

output_dir_name = output.$(BUILD_BOARD)
ifeq ($(O),)
O := $(s)$(output_dir_name)
endif

# ditto for $(O) 
o := $(abspath $(O))/
i := $(o)install/

$(T)/top_defs.mk: ;
include $(T)/top_defs.mk
include $(T)/conf.mk
include $(T)/cmd.mk

###############################################################################
# Logging and output
###############################################################################
# check if the -s flags was passed in
SILENT := $(if $(findstring s,$(MAKEFLAGS)),$(TRUE))
ifeq ($(SILENT),$(TRUE))
_tubs_log := 
else
SILENT :=
_tubs_log = $(info [TUBS] $(1))
endif


_tubs_error = $(__tr2)$(if $(SILENT),,$(error $(call _target_to_makefile,$(1)): $(2)))
# $1 = targetname
# $2 = check, error out if set
# $3 = msg to display on error
_tubs_assert = $(__tr3)$(if $(3),$(if $(2),$(call _tubs_error,$(1),$(3))))

# $1 = target-name
# $2 = what to check
# Make sure that $2 doesn't contain any '$' chars in it
assert_no_dollar = $(__tr3)$(call _tubs_assert,$(1),$(if $(findstring $(dollar),$(2)),$(TRUE)),$(2) called with a dollar sign in argument)

log_file := $(s)make-$(BUILD_BOARD).log

# default is just expand what ever is passed in
ar = $(1)
ard = $(info $(1))$(1)

ifeq ($(SILENT),$(FALSE))

ifeq ($(TUBS_TRACE),$(TRUE))
__tr1 = $(info $(0)($(1)))
__tr2 = $(info $(0)($(1),$(2)))
__tr3 = $(info $(0)($(1),$(2),$(3)))
endif

ifeq ($(or $(TUBS_TRACE),$(TUBS_TRACE_SET)),$(TRUE))
__set_tr1 = $(info $(0)($(1)))
__set_tr2 = $(info $(0)($(1),$(2)))
__set_tr3 = $(info $(0)($(1),$(2),$(3)))
endif

ifeq ($(or $(TUBS_TRACE),$(TUBS_TRACE_STACK)),$(TRUE))
__stack_tr1 = $(info $(0)($(1)))
__stack_tr2 = $(info $(0)($(1),$(2)))
__stack_tr3 = $(info $(0)($(1),$(2),$(3)))
endif

ifeq ($(or $(TUBS_TRACE),$(TUBS_TRACE_CMD)),$(TRUE))
__cmd_tr1 = $(info $(0)($(1)))
__cmd_tr2 = $(info $(0)($(1),$(2)))
__cmd_tr3 = $(info $(0)($(1),$(2),$(3)))
__cmd_tr4 = $(info $(0)($(1),$(2),$(3),$(4)))
__cmd_tr5 = $(info $(0)($(1),$(2),$(3),$(4),$(5)))
__cmd_tr6 = $(info $(0)($(1),$(2),$(3),$(4),$(5),$(6)))
__cmd_tr7 = $(info $(0)($(1),$(2),$(3),$(4),$(5),$(6),$(7)))
endif

ifeq ($(or $(TUBS_TRACE),$(TUBS_TRACE_RULE)),$(TRUE))
ifneq ($(TUBS_TRACE_FANCY),$(TRUE))
__rul_tr1 = $(info $(0)($(1)))
__rul_tr2 = $(info $(0)($(1),$(2)))
__rul_tr3 = $(info $(0)($(1),$(2),$(3)))
__rul_tr4 = $(info $(0)($(1),$(2),$(3),$(4)))
__rul_tr5 = $(info $(0)($(1),$(2),$(3),$(4),$(5)))
__rul_tr6 = $(info $(0)($(1),$(2),$(3),$(4),$(5),$(6)))
__rul_tr7 = $(info $(0)($(1),$(2),$(3),$(4),$(5),$(6),$(7)))
else
__rul_tr1 = $(info $(0)($($(1)_path)))
__rul_tr2 = $(info $(0)($($(1)_path),$(2)))
__rul_tr3 = $(info $(0)($($(1)_path),$(2),$(3)))
__rul_tr4 = $(info $(0)($($(1)_path),$(2),$(3),$(4)))
__rul_tr5 = $(info $(0)($($(1)_path),$(2),$(3),$(4),$(5)))
__rul_tr6 = $(info $(0)($($(1)_path),$(2),$(3),$(4),$(5),$(6)))
__rul_tr7 = $(info $(0)($($(1)_path),$(2),$(3),$(4),$(5),$(6),$(7)))
endif
endif

# these are awlays available as longs at it is not silent mode
__str1 = $(info $(0)($(1)))
__str2 = $(info $(0)($(1),$(2)))
__str3 = $(info $(0)($(1),$(2),$(3)))
__str4 = $(info $(0)($(1),$(2),$(3),$(4)))
__str5 = $(info $(0)($(1),$(2),$(3),$(4),$(5)))
__str6 = $(info $(0)($(1),$(2),$(3),$(4),$(5),$(6)))
__str7 = $(info $(0)($(1),$(2),$(3),$(4),$(5),$(6),$(7)))

ifeq ($(or $(TUBS_TRACE),$(TUBS_TRACE_DEPS)),$(TRUE))
ar = $(info $(1))$(1)
endif

endif # ifeq ($(SILENT),$(FALSE))

ifneq ($(SILENT),$(TRUE))
$(file >>$(log_file),###############################################################################)
$(file >>$(log_file),Builid starting $(shell date))
$(file >>$(log_file),###############################################################################)
$(if $(Q),,$(file >>$(log_file), Build output sent to screen because 'Q' variable is empty))
endif


###############################################################################
# Initialization
###############################################################################
# targets invoke external files to produce a target
external_targets :=
# target has post-makfiles defined. These makefile are included after all
# variabes have been defined.
post_makefile_targets :=
# targets that produce compiled binaries
app_targets :=
# targets that prodcue libraries
lib_targets :=
# all of the subdirectory targets
subdir_targets :=
# list of install-root-names, the default install_root of '/' is not represented in the listed
install_roots :=

# these two variables keep a list of all known directories to be created under output and install
# directories. It is used to construct all of the mkdir rules under those two directories. This
all_extra_output_dirs  :=
all_extra_install_dirs :=

# allow these variables to be set to skip resolving library/include dependencies.
# useful for speeding up running of TUBS for shell interaction
skip_libraries_resolve ?= n
skip_includes_resolve ?= n
skip_dupe_check ?= n
skip_system_checks ?= n

# force silence for certain targets
ifneq ($(filter targets,$(MAKECMDGOALS)),)
skip_libraries_resolve = y
skip_includes_resolve = y
skip_system_checks = y
skip_dupe_check = y
endif

# list of names of loaded sub-makefiles from tubs.d
# each name should be unique, and should be added to, to this variable
# by the .mk themselves.
# TUBS will use the contents of this variable to load and process things
# on behalf of the sub-makefile. See comment right before the files
# are include bellow.
_TUBS_LOADED_SUB_MAKES :=

# list of known allowed subdir targets (ie. dir/some/path/clean)
# this list can be expanded by makefiles included from tubs.d, in that
# case the make should also define a variable (<nameoftarget>_sub_rules)
# that will be expanded to generate the code to handle the sub-rules. 
#For example, for the 'all' target it would be 'all_sub_rules' and
# for for 'vscode' it would be 'vscode_sub_rules'
_TUBS_ALLOWED_SUBDIR_TARGETS := all clean distclean cleandeps as_target install
_TUBS_ALLOWED_TOP_TARGETS := all targets clean distclean cleandeps install download_toolchain

_TUBS_VERSIONS :=
# These two variables hold target and dir stack. These are used during
# loading of all of the project's Makefile to keep track of where
# in the tree we currently exist.

target_stack     :=
directory_stack  :=

# By defualt all target get _ext_build assigned to none,
# define empty variables to keep our error mechanism happy
ext_cmd_configure_none =
ext_cmd_build_none =
ext_cmd_install_none =
ext_cmd_clean_none =
ext_cmd_distclean_none =

# Other ext_cmd_xxx that can be define, these only make
# sense when define command that you want to run ontop of the exiting
# external build system integration
# ext_cmd_pre_configure_<target>
# ext_cmd_post_configure_<target>
# ext_cmd_pre_build_<target>
# ext_cmd_post_build_<target>
# ext_cmd_pre_install_<target>
# ext_cmd_post_install_<target>
# ext_cmd_clean_<target>
# ext_cmd_cleandeps_<target>
# ext_cmd_distclean_<target>
# ext_cmd_setup_<target>
#   called during setup_externals
# ext_cmd_include_external_<target>
#   called when an external Makefile is being processed


# $1 = target-name
# $2 = name for action part of the ext_cmd function to search for
# Expand contents of ext_cmd_ variable based on target's name, NOT the
# ext_build variable. Used when needing to access the target's local
# variables
_ext_name_cmd = $(__cmd_tr2)$(if $(ext_cmd_$(2)_$($(1)_name)),$(call ext_cmd_$(2)_$($(1)_name),$(1)))
# $1 = target-name
# $2 = name for action part of the ext_cmd function to search for
# expands the contents of one of the ext_cmd_ variables. This version
# is meant to be used when internally supported external build integration
# is used
_ext_build_cmd = $(__cmd_tr2)$(if $(call sne,$(origin ext_cmd_$(2)_$($(1)_ext_build)),undefined),$(call ext_cmd_$(2)_$($(1)_ext_build),$(1)))

# $1 = target-name
# $2 = flag file
# $3 = new flags
# compares $3 with contents of file $2, if it exists, and create/update file if they are different
# add a little bit of text infront of $3 when using the value to make sure that the .flag file
# is always created
_check_and_store_flags = $(if $(call sne,$(if $(wildcard $(2)),$(strip $(file <$(2)))),$(strip _tubs_tag_$(3))),$(call cmd,echo,$(1),Updating $(2))$(file >$(2),_tubs_tag_$(3)))
                                
#$(file >$(2),$(3)))
# $1 name of file
# add .flags to passed file name
_flag_file = $(1).flags

###############################################################################
# stack manipulation functions
###############################################################################
# $1 = value of current stack
# $2 = value to add to stack
# return new stack value (that should be assigned back to the stack variable)
push = $(__stack_tr2)$(2:%=$(1):%)
# $1 value of current stack
# returns new stack value
pop  = $(__stack_tr1)$(1:%:$(lastword $(subst :,$(space),$(1)))=%)
peek = $(__stack_tr1)$(lastword $(subst :,$(space),$(1)))
# create a name that is safe to add into a stack
# Replaces spaces and colons with under scores
safe_name = $(subst :,_,$(subst $(space),_,$(1)))
# conver the entire contents of a stack variable to safe name
stack_to_safe_name = $(call safe_name,$(patsubst :%,%,$(1)))
# convert the contents of a stack variable into a path
stack_to_path = $(subst :,/,$(patsubst :%,%,$(patsubst :all%,%,$(1))))
# $1 - path
# $2 - prefix to add ('_' after the prefix will be added by the functioin)
# converts path to a valid variable name, add $(2) as prefix
_path_to_name = $(strip $(patsubst %_,%,$(2)$(subst /,_,$(subst $(space),_,$(1)))))
# converts path to target-name
_path_to_target = $(call _path_to_name,$(1),all_)
# converts a file-system path string to a target name
path_to_target = $(call _path_to_target,$(1))
_relative_path_to_target = $(call _path_to_target,$(patsubst $(S)%,%,$(abspath $($(1)_path)/$(2))))
# converts a relative file-system path to a target name
# $1 = target
# $2 = path to resolve
relative_path_to_target = $(if $(filter ../% ./%,$(2)),$(call _relative_path_to_target,$(1),$(2)),$(call _path_to_target,$(2)))

###############################################################################
#  "associateve" arrays major inspiration comes from gmsl
###############################################################################
__aa_sep := 015AAE3ECCB968957CC0371C14E605EC

# $1 = string
# $2 = other string string
# returns $(TRUE) if two strings are the same
seq = $(__set_tr2)$(if $(subst x$(1),,x$(2))$(subst x$(2),,x$(1)),$(FALSE),$(TRUE))
# returns $(TRUE) if two strings are NOT the same
sne = $(__set_tr2)$(call not,$(call seq,$(1),$(2)))
not = $(__set_tr2)$(if $(1),$(FALSE),$(TRUE))

# $1 = name of array
# $2 = kay
# $3 = value
# set key to value in array
set = $(__set_tr3)$(eval __aa_$(1)_$(__aa_sep)_$(2)_aa_$(1) := $(3))
#set = $(__tr3)$(warning (eval __aa_$(1)_$(__aa_sep)_$(2)_aa_$(1) := $(3)))$(eval __aa_$(1)_$(__aa_sep)_$(2)_aa_$(1) := $(3))

# $1 = name of array
# $2 = key
# returns value associated with key (if any)
get = $(__set_tr2)$(strip $(__aa_$(1)_$(__aa_sep)_$(2)_aa_$(1)))
#get = $(strip $(warning get:(__aa_$(1)_$(__aa_sep)_$(2)_aa_$(1)))$(__aa_$(1)_$(__aa_sep)_$(2)_aa_$(1)))

# $1 = name of array
# return list of all keys in array
keys = $(__set_tr1)$(sort $(patsubst __aa_$(1)_$(__aa_sep)_%_aa_$(1),%,$(filter __aa_$(1)_$(__aa_sep)_%_aa_$(1),$(.VARIABLES))))

# $1 = name of array
# return list of all variabes belonging to the array
_keys = $(__set_tr1)$(sort $(filter __aa_$(1)_$(__aa_sep)_%_aa_$(1),$(.VARIABLES)))

# $1 = name of array
# $2 = key
# return $(TRUE) if key is defined
defined = $(__set_tr2)$(call sne,$(call get,$(1),$(2)),)
#defined = $(call sne,$(call get,$(1),$(2)),)$(warning sne:$(call sne,$(call get,$(1),$(2)),))

# $1 = name of array
clear = $(__set_tr1)$(foreach _k,$(call _keys,$(1)),$(eval undefine $(_k)))

# specialized version of above for accessing output to target-name
# mapping grouped by know install-roots
# $1 = target-name
# the other arguments are same as un-specialized version
_ir_set     = $(__set_tr3)$(call set,ir_$($(1)_install_root),$(2),$(3))
_ir_get     = $(__set_tr2)$(call get,ir_$($(1)_install_root),$(2))
_ir_keys    = $(__set_tr2)$(call keys,ir_$($(1)_install_root),$(2))
_ir_defined = $(__set_tr2)$(call defined,ir_$($(1)_install_root),$(2))

# specialized version for mapping resolved dependencies to their original name
# $1 = target-name
# $2 = resolved dep target-name
# $3 = original name
_rd_set     = $(__set_tr3)$(call set,rd_$(1),$(2),$(3))
_rd_get     = $(__set_tr2)$(call get,rd_$(1),$(2))
_rd_keys    = $(__set_tr2)$(call keys,rd_$(1),$(2))
_rd_defined = $(__set_tr2)$(call defined,rd_$(1),$(2))

# $1 = install-root-name, or the name of the variable as it is generated
#   supposed to be prefixed with install_root_
_ir_get_ir_name = $(__set_tr2)$(call get,ir_$(patsubst _%,%,$(patsubst install_root%,%,$(1))),$(2))

###############################################################################
# Utilities to recusively collect data from target(s)
###############################################################################

# $1 = directory name
# generates list of all directories needed to be able to create the named dir in $1
# ie. lib/module/extra -> 'lib lib/module lib/module/extra'
all_dirs = $(filter-out lib bin,$(sort $(1) $(if $(patsubst .%,%,$(patsubst %/,%,$(dir $(1)))),$(call all_dirs,$(patsubst %/,%,$(dir $(1)))))))

# $1 = target-name
# $2 = variable to expand 
# $3 = passed in as second argument when $2 is expanded (optional)
# $4 = passed in as third argument when $2 is expanded (optional)
# $5 = passed in as fourth argument when $2 is expanded (optional)
# Iterates over targets starting with taget-name $(call)ing $(2) with the current target name as first argument
# breadth-first, not really but close enough
define for_each_target_breadth
$(call $(2),$(1),$(3),$(4),$(5))
$(foreach ___t,$($1_targets),$(call for_each_target_breadth,$(___t),$(2),$(3),$(4),$(5)))
endef

for_all_target_breadth = $(strip $(1) $(foreach __t,$($1_targets),$(call for_all_target_breadth,$(__t),$(2),$(3),$(4),$(5))))

# like above but depth first
define for_each_target_depth
$(foreach ___t,$($1_targets),$(call for_each_target_depth,$(___t),$(2),$(3),$(4),$(5)))
$(call $(2),$(1),$(3),$(4),$(5))
endef

# $1 = target-name
# $2 = name of variable to look for
# $3 = variable to expand for each instance found passing name of target
# $4 = optional, pass as an additional argument when calling $3
# Starting at $1 locate all targets defining target-name_$2 variable and 
# for each instance found call $3 with first argument being the target-name
# found with the variable.
_for_all_vars_breadth = $(if $($(1)_$(2)),$(call $(3),$(1),$(4)))
for_all_vars_breadth = $(strip $(call for_each_target_breadth,$(1),_for_all_vars_breadth,$(2),$(3),$(4)))

# helper to expand target variable, useful for for_all_vars_breadth/depth
_expand_target_var = $($(1)_$(2))
_expand_var = $(__trs1)$(1)

# $1 = target-name
# Starting at $1 return all targets as list using depth-first-order
get_all_targets_depth = $(strip $(foreach _t,$($(1)_targets),$(call get_all_targets_depth,$(_t))) $(1))

# $1 = target-name
# $2 = list of variables (yes, a list)
# $3 = expanded with $(call $(1)) and output is used a prefix to each variable in a target
# $4 = expanded with $(call $(1)) and output is used a prefix to each item output
# Gets contents of multiple variables for a target
# get_var_contents = $(strip $(foreach _v,$(2),$(if $($(1)_$(_v)),$(if $(3),$(call $(3),$(1)))$(foreach _o,$($(1)_$(_v)),$(if $(4),$(call $(4),$(1)))$(_o)))))
# Wrapper for _get_var_contents. Uses depth first variant for_all_targets.
# Get contents of multiple variables for the target and all of it's children
#get_all_var_contents = $(strip $(call for_each_target_depth,$(1),get_var_contents,$(2),$(3),$(4)))

# $1 = target-name
# $2 = variable name
get_all_var_contents = $(strip $(foreach _v,$(subdir_targets) $(lib_targets) $(app_targets) $(external_targets),$($(_v)_$(1))))
get_all_var_contents_shell = $(strip $(foreach _v,$(subdir_targets) $(lib_targets) $(app_targets) $(external_targets),$(_v)_$(1)=\"$($(_v)_$(1))\"))

# $1 = target-name that is being searched
# $2 = what to search for
# returns the target-name if $($(target-name)_outputs) contains $2
_find_target_with_output = $(if $(findstring $(2),$(call _get_build_outputs,$(1))),$(1))

# $1 = what to search for
# Locates targets that procude the file $1 as their output
#find_target_with_output = $(call for_all_vars_breadth,all,outputs,_find_target_with_output,$(1))
#find_tubs_target_with_output = $(strip $(foreach _t,$(strip $(app_targets) $(lib_targets) $(external_targets)),$(call _find_target_with_output,$(_t),$(1))))
find_tubs_target_with_output = $(strip $(if $(call _ir_get,all,$(1)),\
                                    $(call _ir_get,all,$(1)),\
                                    $(foreach _tt,$(install_roots),$(call _ir_get_ir_name,$(_tt),$(1)))))

# $1 = target-name
# $2 = target-path to resolve
# $3 = the source of $2 when display and error
# Attempts to resolve (error out on failure) a path, to a target-name. The path
# could possibly be relative (includes ../) in that case it will be resolved
# relative to $1
find_target = $(strip $(if $($(call path_to_target,$(2))),\
                       $($(call path_to_target,$(2))),\
                       $(if $($(call relative_path_to_target,$($(1)_parent),$(2))),\
                           $(call relative_path_to_target,$($(1)_parent),$(2)),\
                           $(if $($(call relative_path_to_target,$(1),$(2))),\
                                $(call relative_path_to_target,$(1),$(2)),\
                                $(call _tubs_error,$(1),can't resolve dependency '$(2)' in '$(3)'. Should be a valid path to a source dir with a TUBS Makefile)))))

# $1 = target-name
# $2 = resolved target-name
# $3 = original name used to resolve $2
# used to store away original name that was used to resolve a dependency of a target
_memize_rd = $(if $(2),$(call _rd_set,$(1),$(3),$(2)))$(3)

# $1 = target-name
# $2 = target-path of dependency, can be complete path or name of an output by a target, or a relative path
# $3 = suffix to use when display error (name of the variable where the data came from)
# Find a target that matches the path specified by $2.
# like find_target but before attempting to do a relative search, search try match
# libname.so like names passed in $2 to outputs in targets
find_tubs_target_with_libname = $(call _memize_rd,$(1),$(2),$(strip $(if $($(call path_to_target,$(2))),\
                                        $($(call path_to_target,$(2))),\
                                        $(if $(call find_tubs_target_with_output,$(2)),\
                                            $(call find_tubs_target_with_output,$(2)),\
                                            $(if $($(call relative_path_to_target,$($(1)_parent),$(2))),\
                                                $(call relative_path_to_target,$($(1)_parent),$(2)),\
                                                $(error $(call _target_to_makefile,$(1)): can't resolve dependency '$(2)' in '$(3)'))))))

find_target_with_libname = $(strip $(if $($(call path_to_target,$(2))),\
                                    $($(call path_to_target,$(2))),\
                                    $(if $(call find_tubs_target_with_output,$(2)),\
                                        $(call find_tubs_target_with_output,$(2)),\
                                        $(if $($(call relative_path_to_target,$($(1)_parent),$(2))),\
                                            $(call relative_path_to_target,$($(1)_parent),$(2)),\
                                            $(error $(call _target_to_makefile,$(1)): can't resolve dependency '$(2)' in '$(3)')))))

# $1 = target-name
# Generates a sorted list all targets that target-name depends on, including their dependecies...
for_all_targets_includes_deps = $(sort $(foreach _t,$($(1)_tubs_includes),$(_t) $(call for_all_targets_includes_deps,$(_t))))
# $1 = target-name
# Generates a sorted list all targets that target-name depends on directly
all_targets_includes_deps = $(sort $(foreach _t,$($(1)_tubs_includes),$(_t)))

for_all_targets_libraries_deps = $(sort $(foreach _t,$($(1)_tubs_libraries) $($(1)_tubs_libraries_only),$(_t) $(call for_all_targets_libraries_deps,$(_t))))
all_targets_libraries_deps = $(sort $(foreach _t,$($(1)_tubs_libraries),$(_t)))

for_all_targets_deps = $(sort $(foreach _t,$(sort $($(1)_tubs_libraries) $($(1)_tubs_includes)),$(_t) $(call for_all_targets_deps,$(_t))))
all_targets_deps = $(sort $(foreach _t,$($(1)_tubs_libraries) $($(1)_tubs_includes),$(_t)))

# $1 - target-name
# This version of the function will gather include dependencies of $1 and all sub-dirs of $1
# can't think of a single non debug use..
for_all_targets_includes_deps_recursive = $(strip $(foreach _tb,$(call for_all_target_breadth,$(1)),$(call for_all_targets_includes_deps,$(_tb))))

###############################################################################
# Utilities for getting information about targets
###############################################################################

# All function take target-name as $1, some takes extra arguments

_target_to_makefile = $(if $($(1)_dir),$($(1)_dir)/)Makefile
# returns location of target's patch directories, 
# $2 = base directory under patches dir, usually the target-name of
#      a target that produces outputs
_patches_dir = $(strip $(s)$($(1)_dir)/patches/$(2) $(if $(and $($(1)_version),$(wildcard $(s)$($(1)_dir)/patches/$(2)/$($(1)_version))),$(s)$($(1)_dir)/patches/$(2)/$($(1)_version)))
# output dir for target
_o_dir = $(patsubst %/,%,$(o)$($(1)_$(if $($(1)_stamped),path,dir)))
# returns output dir for a target relative to top of build tree
_o_dir_rel = $(patsubst $(s)%,%,$(call _o_dir,$(1)))
# target-name's source location relative to top of source tree
_s_dir = $(patsubst %/,%,$(s)$($(1)_dir))
# target's installation directory (where things should be copied too during install)
# it would be $(i)/$(install_root)/$($(1)_install_as)/$(i)/$(install_root)/lib, $(i)/$(isntall_root)/bin, or $(i) depending on what $(i) is.
_i_dir = $(patsubst %/,%,$(abspath $(i)$(if $($(1)_install_root),$($(1)_install_root)/)$(if $($(1)_install_as),$($(1)_install_as),$(if $(filter app_%,$($(1)_link_type)),bin,$(if $(filter so_%,$($(1)_link_type)),lib)))))
# get the install_root of target
_i_root_dir = $(patsubst %/,%,$(abspath $(i)$(if $($(1)_install_root),$($(1)_install_root))))
# work dir for the current target
_w_dir = $(patsubst %/,%,$(call _o_dir,$(1))/$($(1)_work_dir))
# returns target's unpacked/copied source directory
_ud_dir = $(patsubst %/,%,$(call _w_dir,$(1))/$($(1)_unpacked_dir))
# convert a install-root-name variable to path
_ir_to_dir = $(abspath $(i)$(patsubst _%,%,$(patsubst install_root%,%,$(1))))
_ir_to_path = $(patsubst _%,%,$(patsubst install_root%,%,$(1)))

_opath_dir = $(patsubst %/,%,$(o)$($(1)_path))

# $1 = target-name
# $2 = name of stamp file
_stamp_file = $(call _w_dir,$(1))/tubs-stamp-$(1)-$(2).stamp

_stamp_unpacked     = $(call _stamp_file,$(1),unpacked)
_stamp_patched      = $(call _stamp_file,$(1),patched)
_stamp_configured   = $(call _stamp_file,$(1),configured)
_stamp_built        = $(call _stamp_file,$(1),built)
_stamp_installed    = $(call _stamp_file,$(1),installed)
# path to patch-stamp, this file is created after a patch has been applied successfully
# $2 = name of the patch file
_stamp_patch = $(call _stamp_file,$(1),$(notdir $(lastword $(2))))

# Return base directory for an install-root
_path_to_ir = $(call _path_to_name,$(1),install_root_)

# Returns target's outputs that are produces by tubs
_get_build_outputs = $(strip $(filter-out $($(1)_extra_dirs) $($(1)_extra_files),$($(1)_outputs)))
_get_extra_outputs = $(strip $(filter $($(1)_extra_dirs) $($(1)_extra_files),$($(1)_outputs)))

# $1 = target-name
# $2 = _i_dir/_o_dir/_s_dir
# returns build outputs relative to a directory 
get_build_outputs = $(foreach _o,$(call _get_build_outputs,$(1)),$(call $(2),$(1))/$(_o))

# Handles .c, .cpp
_source_to_object = $(foreach _so,$(2),$(patsubst ./%,%,$(dir $(_so))$($(1)_name)-$(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(notdir $(_so))))))
# converts target's source files into object file and their output directories
_get_sources = $(addprefix $(call _s_dir,$(1))/,$($(1)_sources))
_get_objects = $(addprefix $(call _o_dir,$(1))/,$(call _source_to_object,$(1),$($(1)_sources)))

# $1 = target
# $2 = variable to expand for each file/dir
_get_extra_files = $(addprefix $(call $(2),$(1))/,$(strip $($(1)_extra_files)))
_get_extra_dirs  = $(addprefix $(call $(2),$(1))/,$(strip $($(1)_extra_dirs)))
_get_install_files = $(addprefix $(call _i_dir,$(1))/,$(strip $(call _get_build_outputs,$(1))))\
                     $(addprefix $(call _i_root_dir,$(1))/,$(strip $(call _get_extra_outputs,$(1))))

# $1 = target-name
# $2 = should be _o_dir or _i_dir variable
# Returns paths to each (tubs built) library that target-name depends on
get_target_deps_as_libs = $(strip $(sort $(foreach _d,$($(1)_tubs_libraries) $($(1)_tubs_libraries_only),$(foreach _bo,$(call _get_build_outputs,$($(_d))),$(call $(2),$(_d))/$(_bo)))))

# $1 = target-name
# $2 = should be _o_dir or _i_dir variable
# Returns paths to each (tubs built) target's output/install directory
get_target_deps_output_paths = $(strip $(sort $(foreach _d,$($(1)_tubs_libraries) $($(1)_tubs_libraries_only),$(call $(2),$(_d)))))
get_target_deps_output_rpaths = $(patsubst %/,%,$(sort $(dir $(foreach _tdo,$(call get_target_deps_output_paths,$(1),ar),$(addprefix /,$(call _get_build_outputs,$(_tdo)))))))

# $1 = target-name
# Return list of source dep files for the target (all source files have their extension replaced with .o.d)
get_target_source_deps = $(addsuffix .d,$(call _get_objects,$(1)))

# $1 = target-name
# Return list of target's rpaths.
# Or, target's output prepended to each unique $(dir ) that is found
# in the targets outputs.
get_target_rpaths = $(foreach _tr,$(dir $($(1)_outputs)),$(abspath $(call _o_dir,$(1))/$(_tr)))

# $1 = target-name
# $2 = variable to expand/call for each dependency
# call $2 for each dependency of target-name that uses stamp files
# this is used to generate the list of target's direct and indirect dependecies
# if we need a dir or a object in place before we can use them, this forces
# those target to be built first
for_each_targets_stamped_deps = $(__tr2)$(foreach _d,$(call for_all_targets_deps,$(1)),\
                                    $(if $($(_d)_stamped),$(call $(2),$(_d))))
                                    
# $1 = target-name
# $2 = variable to expand
# $3 = passed as as second argument to $2
# expand $2 for each target that uses stamp files (external) starting
# at $1.
for_all_stamped_targets = $(__tr2)$(strip $(foreach _d,$(call for_all_target_breadth,$(1)),\
                                    $(if $($(_d)_stamped),$(call $(2),$(_d),$(3)))))

#######################################################################################
# Core of the non-recusrive part of the build system
#
# the variable $(_target) is valid (and contains the full name of the current target)
# as long you are staying within the inlclude_subdir call-chain. Same things goes
# for $(_parent) which contains the full target name of the current target's parent.
#
# What is this mess? Why some places two dollar signs other only one?
# 
# There are two expension happening here, once when $(call ) function is called.
# The second when $(eval ) is called.
# Some variables like function variables ($1, $2, etc...) and the _target and _parent
# only exist for a short period of time. To make things less likely not to work for you,
# as a rule of thumb, local variables ($(1), and friends) and the $(_target) and $(_parent) 
# variables should be expanded with a single dollar sign.
# everything else should have two dollar signs to make sure that most of the 
# expension happens during the $(eval ) call.
# There are cases where this is not true, ie. when needing to do a $(if ) call on
# a variables that is defined in the local makefile. That variable could be overwritten
# anywhere down the line, so it should not expect to live for long. In that case
# those parts of the line should not use secondary expension. Yes it's confusing.
######################################################################################

# $1 = target-name
# $2 = Location (path) of the makefile for the new target
define push_target
    # store away the stack as the target-name for parent to use later
    _parent := $$(call stack_to_safe_name,$$(target_stack))
    target_stack := $$(call push,$$(target_stack),$(1))
    dir_stack := $$(call push,$$(dir_stack),$(2))

    # put target-name into _target that way it can be referenced in include_sbudir
    _target := $$(call stack_to_safe_name,$$(target_stack))
    $$(_target) := $$(_target)
    $$(_target)_name = $(1)

    $$(_target)_version := $(if $(_ver),$(_ver),$$($$(_parent)_version))

    # have special case for TOP target, we want the path to be all
    ifeq ($$(_target),all)
    $$(_target)_dir :=
    $$(_target)_path := all
    else
    $$(_target)_dir := $$(patsubst $(s)%,%,$$(call peek,$$(dir_stack)))
    $$(_target)_path := $$(call stack_to_path,$$(target_stack))
    endif
    $$(_target)_parent := $$(_parent)
    $$(_parent)_targets := $$($$(_parent)_targets) $$(_target)
endef

# $1 = target-name
define include_post_makefile
# variables to make post-makefiles a bit easier to write
_s := $(call _s_dir,$(1))
_o := $(call _o_dir,$(1))
_w := $(call _w_dir,$(1))
_i := $(call _i_dir,$(1))
_t := $(1)

$(if $(wildcard $(if $($(1)_dir),$($(1)_dir)/)$($(1)_post_makefile)),,(error $(if $($(1)_dir),$($(1)_dir)/)Makefile: post-makefile '$(if $($(1)_dir),$($(1)_dir)/)$($(1)_post_makefile)' not found specified in '$($(1)_name)_post_makefile'))
include $(if $($(1)_dir),$($(1)_dir)/)$($(1)_post_makefile)
endef

# no argumnents
# Pop the current target from the stack.
# This resets the _target _parent target_stack and dir_stack.
define pop_target
    # see if the target defines a post makefile
    ifneq ($($(_target)_post_makefile),)
        $$(eval post_makefile_targets += $(_target))
    endif

    _target := $$(_parent)
    _parent := $$($$(_parent)_parent)
    target_stack := $$(call pop,$$(target_stack))
    dir_stack := $$(call pop,$$(dir_stack))
endef

define _copy_makefile_variable
    $(_target)_$(2) := $(if $(4),$(call $(4),$($(1)_$(3))),$($(1)_$(3)))
endef

define _set_variable
    $(_target)_$(1) := $(if $(3),$(call $(3),$(2)),$(2))
endef

# $1 = target name in Makefile
# $2 = variable name
# $3 = (optional) value to use if variable is not defined in the makefile
# $4 = transform each value in $($(1)_$(2)) by expanding it the variable
#      passed in (ie. $(call $(5),$($(1)_$(2))))
# copy target variable from the Makefile into the global namespace
# set variable [$($(_target)_$(2))] to the value of variable [$($(1)_$(2)] in the Makefile
define copy_makefile_variable
    $$(if $($(1)_$(2)),\
        $$(eval $$(call _copy_makefile_variable,$(1),$(2),$(2),$(4))),\
        $$(if $(3),\
            $$(eval $$(call _set_variable,$(2),$(3),$(4)))))
endef

# $1 = target name in Makefile
# $2 = variable name
# $3 = copy to variable
# $4 = (optional) value to use if variable is not defined in the makefile
# $5 = transform each value in $($(1)_$(2)) by expanding it the variable
#      passed in (ie. $(call $(5),$($(1)_$(2))))
# like copy_makefile_variable but specified the target variable's name
# set variable $($(_target)_$(3)) to the value of variable $($(1)_$(2) in the Makefile
define copy_makefile_variable_to_named
    $$(if $($(1)_$(2)),\
        $$(eval $$(call _copy_makefile_variable,$(1),$(3),$(2),$(5))),\
	    $$(if $(4),\
            $$(eval $$(call _set_variable,$(3),$(4),$(5)))))
endef

__vars_to_sanitize := sources
_xform_i_dir = $(foreach __i,$(1),$(call _i_dir,$(_target))/$(__i))
_xform_o_dir = $(foreach __i,$(1),$(call _o_dir,$(_target))/$(__i))

# $1 = target name in Makefile
# 
define setup_common_variables
    $$(if $($(1)_outputs),$$(eval $$(call copy_makefile_variable,$(1),outputs)))
    $$(if $($(1)_post_makefile),$$(eval $$(call copy_makefile_variable,$(1),post_makefile)))
    $$(if $($(1)_post_clean_rules),$$(eval $$(call copy_makefile_variable,$(1),post_clean_rules)))
    $$(if $($(1)_extra_files),$$(eval $$(call copy_makefile_variable,$(1),extra_files)))
    $$(if $($(1)_extra_dirs),$$(eval $$(call copy_makefile_variable,$(1),extra_dirs)))
    $$(eval $$(call copy_makefile_variable,$(1),version,$(if $($(_target)_version),$($(_target)_version),0.0.0)))
    $$(if $($(1)_libraries),$$(eval $$(call copy_makefile_variable,$(1),libraries)))
    $$(if $($(1)_libraries_only),$$(eval $$(call copy_makefile_variable_to_named,$(1),libraries_only,libraries_only)))
    $$(if $($(1)_includes),$$(eval $$(call copy_makefile_variable,$(1),includes)))
    $$(if $($(1)_extra_clean_dirs),$$(eval $$(call copy_makefile_variable_to_named,$(1),extra_clean_dirs,clean_dirs)))
    $$(if $($(1)_extra_clean_files),$$(eval $$(call copy_makefile_variable_to_named,$(1),extra_clean_files,clean_files)))
    $$(if $($(1)_export_includes),$$(eval $$(call copy_makefile_variable,$(1),export_includes)))
    $$(if $($(1)_export_libraries),$$(eval $$(call copy_makefile_variable,$(1),export_libraries)))
    $$(eval $$(call copy_makefile_variable,$(1),perm_mode))
    $$(eval $$(call copy_makefile_variable,$(1),user,root))
    $$(eval $$(call copy_makefile_variable,$(1),group,root))

    $$(eval $$(call copy_makefile_variable,$(1),install_root,))
    $$(eval $$(call _path_to_ir,$($(1)_install_root)) += $(_target))
    $$(eval install_roots := $$(strip $$(sort $$(install_roots) $(call _path_to_ir,$($(1)_install_root)))))
endef

define setup_patch_variables
$(1)_patch_files := $(strip $($(1)_patch_files) $(wildcard $(addsuffix /*.patch,$($(1)_patch_dirs))))
endef

# $1 = target name in Makefile
define include_external
    $$(eval $$(call setup_common_variables,$(1)))
    external_targets += $$(_target)
    # setup patch variables
    $$(eval $$(call copy_makefile_variable,$(1),patch_dirs,$$(wildcard $$(call _patches_dir,$(_target),$(1)))))
    $$(eval $$(call copy_makefile_variable,$(1),patch_files))
    # setup patch files from patch dirs
    $$(if $$($(_target)_patch_dirs),$$(eval $$(call setup_patch_variables,$(_target))))
    $$(if $$($(_target)_outputs),,$$(error $$(call _target_to_makefile,$(1)): external target '$(1)' does not define any outputs)) 
    $$(if $$($(1)_source_archive)$$($(1)_source_dir)$$($(1)_source_skip),,$$(error $$(call _target_to_makefile,$(1)): $(1) doesn't define $(1)_source_archive, $(1)_source_dir, or $(1)_source_skip))
    $$(eval $$(call copy_makefile_variable,$(1),work_dir,work))
    $$(eval $$(call copy_makefile_variable,$(1),ext_build,$$($(_target)_name)))
    
ifneq ($(SILENT),$(TRUE))
    $$(if $($(1)_ext_build),$$(if $$(call seq,undefined,$$(origin ext_cmd_install_$($(1)_ext_build))),$$(warning $$(call _target_to_makefile,$(_target)): defines an unknown $(1)_ext_build value, either define ext_cmd_install_$$($(_target)_ext_build) or use one of built in types)))
    $$(if $($(1)_ext_build),$$(if $$(call seq,undefined,$$(origin ext_cmd_build_$($(1)_ext_build))),$$(warning $$(call _target_to_makefile,$(_target)): defines an unknown $(1)_ext_build value, either define ext_cmd_build_$$($(_target)_ext_build) or use one of built in types)))
    $$(if $($(1)_ext_build),$$(if $$(call seq,undefined,$$(origin ext_cmd_configure_$($(1)_ext_build))),$$(warning $$(call _target_to_makefile,$(_target)): defines an unknown $(1)_ext_build value, either define ext_cmd_configure_$$($(_target)_ext_build) or use one of built in types)))
    $$(if $($(1)_ext_build),$$(if $$(call seq,undefined,$$(origin ext_cmd_clean_$($(1)_ext_build))),$$(warning $$(call _target_to_makefile,$(_target)): defines an unknown $(1)_ext_build value, either define ext_cmd_clean_$$($(_target)_ext_build) or use one of built in types)))
    $$(if $($(1)_ext_build),$$(if $$(call seq,undefined,$$(origin ext_cmd_distclean_$($(1)_ext_build))),$$(warning $$(call _target_to_makefile,$(_target)): defines an unknown $(1)_ext_build value, either define ext_cmd_distclean_$$($(_target)_ext_build) or use one of built in types)))
endif

    $$(if $($(1)_ext_build),$$(eval $$(call ext_cmd_include_external_$($(1)_ext_build),$(1))))

    $$(eval $$(call copy_makefile_variable,$(1),source_archive))
    $$(if $($(1)_source_archive),$$(if $$($(1)_unpacked_dir),,$$(call _tubs_error,$(_target),$(1)_unpacked_dir must be defined with $(1)_source_archive)))
    $$(eval $$(call copy_makefile_variable,$(1),unpacked_dir))

    $$(eval $$(call copy_makefile_variable,$(1),work_dir))

    $$(eval $$(call copy_makefile_variable,$(1),source_dir))
    $$(if $($(1)_source_dir),$$(if $$($(1)_unpacked_dir),,$$(eval $(_target)_unpacked_dir := $($(1)_source_dir))))

    $$(eval $$(call copy_makefile_variable,$(1),conf_options))
    $$(eval $$(call copy_makefile_variable,$(1),cmd_env))

    $$(eval $$(call copy_makefile_variable,$(1),use_destdir,$(FALSE)))

    $$(eval $$(call copy_makefile_variable,$(1),url))
    $$(eval $$(call copy_makefile_variable,$(1),hash))
    $$(eval $$(call copy_makefile_variable,$(1),hash_type))

    $$(if $$($(_target)_hash),$$(if $$($(_target)_hash_type),,$$(call _tubs_error,$(_target),$(1)_hash_type must be defined)))
    $$(if $$($(_target)_hash_type),$$(if $$(call cmd_hash_$$($(_target)_hash_type)),,$$(call _tubs_error,$(_target),$(1)_hash_type has invalid type valid values: $$(patsubst cmd_hash_%,'%',$$(filter cmd_hash_%,$$(.VARIABLES))))))
    $$(if $$($(_target)_outputs),,$$(call _tubs_error,$(_target),external target '$(1)' does not define any outputs))

    $$(eval $(_target)_export_libraries := $$(sort $$($(_target)_export_libraries) $$(patsubst %/,%,$$(dir $$(filter %.so %.a,$$($(_target)_outputs))))))
    $$(eval $(_target)_stamped = $(TRUE))

    $$(eval $(_target)_clean_files += $$(call _stamp_built,$(_target)))
endef
 #$(s)$$($(_target)_dir)/patches/$(1)/$($(_target)_version))))

# $1 = target name in Makefile
# Setup/validate variables that are used to specify a target that TUBS knows how to
# build itself, ie. app/libraries.
#
define _include_tubs_built
    $$(eval $$(call setup_common_variables,$(1)))

    $$(if $($(1)_sources),$$(eval $$(if $$(findstring *,$($(1)_sources)),$$(error $$($(_target)_dir)/Makefile: '$(1)_sources' contains wildcards, that goes into $(1)_sources_glob))))
    $$(if $($(1)_sources),$$(eval $(_target)_sources := $$(sort $$($(_target)_sources) $(strip $($(1)_sources)))))
    $$(if $($(1)_sources_glob),$$(eval $(_target)_sources := $$(sort $$($(_target)_sources) $(patsubst $($(_target)_dir)/%,%,$(wildcard $(patsubst %,$($(_target)_dir)/%,$($(1)_sources_glob)))))))
    $$(eval all_extra_output_dirs += $$(strip $$(addprefix $$($(_target)_dir)/,$$(patsubst %/,%,$$(subst ./,,$$(sort $$(dir $$($(_target)_sources))))))))

    $$(if $$($(_target)_sources),,$$(error $$($(_target)_dir)/Makefile: '$$($(_target)_name)' doesn't define any sources))
    $$(if $$($(_target)_sources),$$(eval $(_target)_clean_files += $$(call _get_objects,$(_target))))

    $$(if $($(1)_local_includes),$$(eval $$(call copy_makefile_variable,$(1),local_includes)))
    $$(if $($(1)_cflags),$$(eval $$(call copy_makefile_variable,$(1),cflags)))
    $$(if $($(1)_cxxflags),$$(eval $$(call copy_makefile_variable,$(1),cxxflags)))
    $$(if $($(1)_ldflags),$$(eval $$(call copy_makefile_variable,$(1),ldflags)))
    $$(if $($(1)_cppflags),$$(eval $$(call copy_makefile_variable,$(1),cppflags)))
    $$(if $($(1)_defines),$$(eval $$(call copy_makefile_variable,$(1),defines)))
endef
# $1 = target name in Makefile
# Setup lib related variables
define include_lib
    $$(eval $$(call _include_tubs_built,$(1)))
    $$(eval lib_targets += $$(_target))
    $$(eval $$(_target)_install_as := lib)
    $$(eval $$(_target)_outputs += lib$(1).so)
    $$(eval $$(_target)_link_type := $$(if $$(filter %.cpp,$$(suffix $$($$(_target)_sources))),so_cpp,so_c))
    $$(if $$($(_target)_perm_mode),,$$(eval $$(_target)_perm_mode := 0775))

endef

# $1 = target name in Makefile
# Setup app building variables
define include_app
    $$(eval $$(call _include_tubs_built,$(1)))
    $$(eval app_targets += $$(_target))
    $$(eval $$(_target)_outputs += $(1))
    $$(if $(2),\
        $$(eval $$(_target)_install_as := $(2)),\
        $$(eval $$(_target)_install_as := bin))

    $$(if $$($(_target)_perm_mode),,$$(eval $$(_target)_perm_mode := 0775))

    $$(eval $$(_target)_link_type := $$(if $$(filter %.cpp,$$(suffix $$($$(_target)_sources))),app_cpp,app_c))
endef

# $1 = target-name
# copies target's outputs into a our output lookup table and check to make sure
# that the outputs doesn't conflict with any other target that installs
# it's output into the same install-root
_process_outputs   = $(__tr3)$(foreach _t,$($(1)_outputs),\
                                    $(eval $(call _tubs_assert,$(1),$(call _ir_defined,$(1),$(_t)),$(_t) produced by '$($(1)_name)' conflicts with output produced in $(call _target_to_makefile,$(call _ir_get,$(1),$(_t))) by '$($(call _ir_get,$(1),$(_t))_name)'))\
                                    $(eval $(call _ir_set,$(1),$(_t),$(1))))

# $1 = target-name
# $2 = directory
define include_subdir
    # get version info
    ifeq ($(SILENT),$(FALSE))
        ifeq ($$(wildcard $(2)/.git),$(2)/.git)
            _ver = $$(shell (cd $(2); $(T)/scripts/git_utils.sh get_build_version))
            $$(call _tubs_log,VERSION: '$(notdir $(patsubst $(S)%,%,$(2))): $$(_ver)')
            $$(eval _TUBS_VERSIONS += $(notdir $(patsubst $(S)%,%,$(2)))=$$(_ver))
        endif
    endif

    # push target for the included directory on the stack first
    $$(eval $$(call push_target,$(1),$(2)))
    # add target-name to subdirs list
    $$(eval subdir_targets += $$(_target))

    ifeq ($$(wildcard $(2)),)
      $$(error $$(if $$($(_target)_dir),$$($(_target)_path)/)Makefile: '$$(patsubst $(s)%,%,$(2))' doesn't exist, specified in subdirs)
    endif

    # provide a useful error message if the makefile for the sub-dir can not be found.
    ifeq ($$(wildcard $(2)/Makefile),)
      $$(error $$(if $$($(_target)_dir),$$($(_target)_path)/)Makefile: $$(patsubst $(s)%,%,$(2))/Makefile not found, specified in subdirs)
    endif

    # clear important variables used inside of makefiles
    empty :=
    subdirs :=
    externals :=
    apps :=
    libs :=
    # clea any variables that match the names in _TUBS_LOADED_SUB_MAKES
    $$(foreach _s,$$(_TUBS_LOADED_SUB_MAKES),$$(eval $$(_s) :=))

    # Prevent auto-rebuild of makefiles
    $(2)/Makefile: ;

    _s := $(2)
    _o := $(o)$($(_target)_dir)
    _i := $(i)
    _ver :=

    # inlucde the makefile
    include $(2)/Makefile
    _s :=

    $$(if $$(strip $$(externals) $$(libs) $$(apps) $$(subdirs) $$(empty)),,\
      $$(error $$(if $$($$(_target)_dir),$$($$(_target)_path)/)Makefile: Needs to define one of 'apps', 'libs', 'subdirs', 'externals'. Included in $$(if $$($$(_parent)_dir),$$($$(_parent)_dir)/)Makefile))

    $$(foreach _a,$$(apps),\
    $$(eval $$(call push_target,$$(notdir $$(_a)),$$($$(_target)_dir)))\
    $$(eval $$(call include_app,$$(notdir $$(_a)),$$(patsubst ./%,,$$(dir $$(_a)))))\
    $$(eval $$(call _process_outputs,$$(_target)))\
    $$(eval $$(call pop_target)))

    $$(foreach _l,$$(libs),\
    $$(eval $$(call push_target,$$(_l),$$($$(_target)_dir)))\
    $$(eval $$(call include_lib,$$(_l)))\
    $$(eval $$(call _process_outputs,$$(_target)))\
    $$(eval $$(call pop_target)))

    $$(foreach _t,$$(externals),\
    $$(eval $$(call push_target,$$(notdir $$(_t)),$$($$(_target)_dir)))\
    $$(eval $$(call include_external,$$(notdir $$(_t))))\
    $$(eval $$(call _process_outputs,$$(_target)))\
    $$(eval $$(call pop_target)))

    $$(foreach _s,$$(_TUBS_LOADED_SUB_MAKES),$$(if $$($$(_s)),$$(foreach _t,$$($$(_s)),$$(eval $$(call $$(_s)_tubs_include_custom,$$(_t))))))

    $$(foreach _d,$$(subdirs),\
    $$(eval $$(call include_subdir,$$(_d),$(2)/$$(_d))))

    # clear important variables used inside of makefiles, again
    # probably don't need to do this anymore...
    empty :=
    subdirs :=
    externals :=
    apps :=
    libs :=
    _ver :=

    # pop the target, 
    $$(eval $$(call pop_target))
endef

###############################################################################
# $(_target) and $(_parent) is no longer valid
###############################################################################

###############################################################################
# Things common to all rules
###############################################################################

# $1 = target-name
# $2 = extra dir to create rules for
define __setup_extra_dirs_install
$(call _i_root_dir,$(1))/$(2): | $(patsubst %/$(notdir $(2)),%,$(call _i_root_dir,$(1))/$(2))
	$$(call cmd,mkdirp,$(1),$$(@),,,test ! -e $(@))
endef

define _setup_extra_dirs_install
$(__rul_str2)

$(if $(call defined,_extra_inst_seen,$(call _i_root_dir,$(1))_$(2)),,\
    $(eval $(call __setup_extra_dirs_install,$(1),$(2)))\
    $(call set,_extra_inst_seen,$(call _i_root_dir,$(1))_$(2),$(TRUE)))
endef

# $1 = target-name
# $2 = extra dir to create, relative $($(1)_dir)
# Create rules to mkdir directories specified in $(target-name)_extra_dirs variable
# Add dependency on parent directory. This means that the parent directory should also
# be listed in $(target-name)_extra_dirs variables.
#XXX: $(3)$(2): | $(patsubst %/$(notdir $(2)),%,$(3)$(2))
define _setup_extra_dirs
$(__rul_str2)
$($(1)_path)/install: $(call _i_root_dir,$(1))/$(2)
$(call _o_dir,$(1))/$(2): | $(patsubst %/$(notdir $(2)),%,$(call _o_dir,$(1))/$(2))
	$$(call cmd,mkdirp,$(1),$$(@),,,test ! -e $(@))

$(if $(or $($(1)_stamped),$($(1)_install_as)),$(eval $(call _setup_extra_dirs_install,$(1),$(2))))
endef

# $1 = target-name
# $2 = path to file/dir
# Output info needed by package creation tools, ie. user/group id and file permissions
# This information is added to files that live in the same directory as the output
# with an extension added depending on what it contains.
# These are used by package creation tool to know what to update during fakeroot and when files are installed directly
# onto the target.
#
# Note the difference in the the leading tabs/spaces at the start of the line.
# The first two line uses space since they are doing a check
# The others are what's going to be executed by the rule
#
# The line dealing with perm mode is only executed if there are not stamp files associated with the target. Eg. not an external
# target.
define _create_install_info
    $(if $($(1)_user),$(if $(call defined,_user_to_id_map,$($(1)_user)),,$(error Don't have know to map user name: '$($(1)_user)' to id. Make sure it exists in TUBS_USER_TO_ID_MAP)))
    $(if $($(1)_group),$(if $(call defined,_group_to_id_map,$($(1)_group)),,$(error Don't have know to map group name: '$($(1)_group)' to id. Make sure it exists in TUBS_GROUP_TO_ID_MAP)))
	$(if $($(1)_user),$(file >$(2).__tubs_user,$(call get,_user_to_id_map,$($(1)_user))))
	$(if $($(1)_group),$(file >$(2).__tubs_group,$(call get,_group_to_id_map,$($(1)_group))))
	$(if $($(1)_perm_mode),$(file >$(2).__tubs_mode,$($(1)_perm_mode)),$(if $($(1)_stamped),,$(file >$(2).__tubs_mode,0664)))
endef

# $1 = target-name
# $2 = path to file/dir
define _install_to_target
	@echo "[ SCP TARGET ] Copying $(2) to root@$(BOARD_IP):$(if $(call seq,$(patsubst $(i)%,%,$(call _i_root_dir,$(1))),config),/mnt/config)$(patsubst $(call _i_root_dir,$(1))%,%,$(2))"
	$(if $(findstring .ssh/authorized_keys,$(2)),,$(if $(Q),@)$(SSH) $(if $(TUBS_BOARD_SSH_KEY),-i $(TUBS_BOARD_SSH_KEY) )-q root@$(BOARD_IP) 'rm -f $(if $(call seq,$(patsubst $(i)%,%,$(call _i_root_dir,$(1))),config),/mnt/config)$(patsubst $(call _i_root_dir,$(1))%,%,$(2))')
	$(if $(Q),@)if test -L "$(2)"; then \
		__link__=`readlink $(2)`; \
		$(SSH) $(if $(TUBS_BOARD_SSH_KEY),-i $(TUBS_BOARD_SSH_KEY) )$(if $(Q),-q) root@$(BOARD_IP) "ln -s $${__link__} $(patsubst $(call _i_root_dir,$(1))%,%,$(2))"; \
	else \
		$(SCP) $(if $(TUBS_BOARD_SSH_KEY),-i $(TUBS_BOARD_SSH_KEY) )$(if $(Q),-q) $(2) root@$(BOARD_IP):$(if $(call seq,$(patsubst $(i)%,%,$(call _i_root_dir,$(1))),config),/mnt/config)$(patsubst $(call _i_root_dir,$(1))%,%,$(2)); \
	fi
	$(if $(wildcard $(2).__tubs_user),$(if $(Q),@)$(SSH) $(if $(TUBS_BOARD_SSH_KEY),-i $(TUBS_BOARD_SSH_KEY) )-q root@$(BOARD_IP) 'chown $(file <$(2).__tubs_user) $(if $(call seq,$(patsubst $(i)%,%,$(call _i_root_dir,$(1))),config),/mnt/config)$(patsubst $(call _i_root_dir,$(1))%,%,$(2))')
	$(if $(wildcard $(2).__tubs_group),$(if $(Q),@)$(SSH) $(if $(TUBS_BOARD_SSH_KEY),-i $(TUBS_BOARD_SSH_KEY) )-q root@$(BOARD_IP) 'chown :$(file <$(2).__tubs_group) $(if $(call seq,$(patsubst $(i)%,%,$(call _i_root_dir,$(1))),config),/mnt/config)$(patsubst $(call _i_root_dir,$(1))%,%,$(2))')
	$(if $(wildcard $(2).__tubs_mode),$(if $(Q),@)$(SSH) $(if $(TUBS_BOARD_SSH_KEY),-i $(TUBS_BOARD_SSH_KEY) )-q root@$(BOARD_IP) 'chmod $(file <$(2).__tubs_mode) $(if $(call seq,$(patsubst $(i)%,%,$(call _i_root_dir,$(1))),config),/mnt/config)$(patsubst $(call _i_root_dir,$(1))%,%,$(2))')
endef

# $1 = target-name
# Generate the command flags (aka dependency on make variable values) for deciding if 
# target needs to be re-installed
define _create_install_flags
$(call _check_and_store_flags,$(1),$(2),$(call get,_user_to_id_map,$($(1)_user))$($(1)_user)$(call get,$($(1)_group))$($(1)_group)$($(1)_perm_mode))
endef

define _add_copy_file_deps
$(call _o_dir,$(1))/$(2): $(call _s_dir,$(1))/$(2)
endef

define _copy_file_rules
$(call _o_dir,$(1))/$(2): $(call _s_dir,$(1))/$(2) |$(patsubst %/,%,$(dir $(call _o_dir,$(1))/$(2)))
	$$(call cmd,cp,$(1),$$@,$$<)
endef

# $1 = target-name
# $2 = the extra file
# Similar to _setup_extra_dirs but for files.
# Generate rules to copy files specified in $(target-name)_extra_files.
# Also generate dependency on the directory that the file will be created in. The directory
# should be listed in $(target-name)_extra_dirs or can be one of the install or output dirs
# for the target.
# Will also generate commands to copy the file to the target if BOARD_IP is defined when installing to the install-root

define _setup_extra_files
$(__rul_tr2)
$($(1)_path)/install: $(call _i_root_dir,$(1))/$(2)

$(call _i_root_dir,$(1))/$(2): $(call _o_dir,$(1))/$(2) $(call _i_root_dir,$(1))/$(2).__tubs_install_flags |$(patsubst %/$(notdir $(2)),%,$(call _i_root_dir,$(1))/$(2)) $(if $(BOARD_IP),$(if $(call seq,$(call _i_root_dir,$(1))/$(2),$(TUBS_BOARD_SSH_KEY)),,$(TUBS_BOARD_SSH_KEY)))
	$$(call cmd,cp,$(1),$$@,$$<)
	$$(call _create_install_info,$(1),$$@)
	$(if $(BOARD_IP),$$(call _install_to_target,$(1),$$@))

$(call _i_root_dir,$(1))/$(2).__tubs_install_flags: .force | $(patsubst %/,%,$(dir $(call _i_root_dir,$(1))/$(2)))
	$$(call _create_install_flags,$(1),$$@)

ifeq ($($(1)_stamped),)
$(if $(wildcard $(call _s_dir,$(1))/$(2)),$(call _add_copy_file_deps,$(1),$(2)))
$(call _o_dir,$(1))/$(2): | $(patsubst %/$(notdir $(2)),%,$(call _o_dir,$(1))/$(2))
	$$(if $$<,$$(call cmd,cp,$(1),$$@,$$<),$$(call cmd,echo,$(1),I do not have a source to copy to $$@, possibly a typo in the file name))
else
$(if $(wildcard $(call _s_dir,$(1))/$(2)),$(call _copy_file_rules,$(1),$(2)))
endif
endef

# $1 = target-name
# Setup rules to handle the "extras" for targets
# NOTE: the dependency between the installation and output is handled by the invoker
# of this functions
define _setup_extras
$(__rul_tr1)
$(foreach _d,$($(1)_extra_dirs),$(eval $(call _setup_extra_dirs,$(1),$(_d))))
$(foreach _f,$($(1)_extra_files),$(eval $(call _setup_extra_files,$(1),$(_f))))
endef

# $1 = target-name
# create dependency tree of paths relative to the source directory
# all:
# in case you were searching for the all target, it's dependencies would 
# be set here when ever this variable is expanded for any subdirs defined
# in the top most Makefile.
# so, in theory it looks like this:
# all: subdir1 subdir2
# subdir1: subdir1/sub_subdir1
# subdir1/sub_dir1: output
# subdir2: ...
# This ends up creating the dependency tree for the subdirectories
# and as well as having each subdirectory depend on outputs defined
# within their own Makefiles. (subdir is a special type of output)
define setup_subdir
$(__rul_tr1)
$($($(1)_parent)_path): $($(1)_path)
$(if $(call for_all_vars_breadth,$(1),outputs,_expand_target_var,outputs),$($($(1)_parent)_path)/install: $($(1)_path)/install)

$(call _o_dir,$(1)): | $(call _o_dir,$($(1)_parent))
$(call _o_dir,$(1)):
	$$(call cmd,mkdirp,$(1),$$(@),,,test ! -e $(@))

endef

# $1 = target-name
# $2 = output produced by the target
# 1. install_dir/output: output_dir/output | dir(output_dir/output) 
# or:install_dir/output: | output_dir/output dir(output_dir/output) 
# The rule produced depends if output comes from extra_dirs or not. If it's a
# directory we want it to only be created, not affect the dependency status
# of the parent.
# 
# Copies target's outputs into the staging area
define setup_outputs_install
$(__rul_tr2)
$($($(1)_parent)_path)/install: $($(1)_path)/install
$($(1)_path)/install: $(call _i_dir,$(1))/$(2)

$(call _i_dir,$(1))/$(2): $(if $(filter $(2),$($(1)_extra_dirs)),\
                            $(call _o_dir,$(1))/$(2) $(call _i_dir,$(1))/$(2).__tubs_install_flags $(patsubst %/,%,$(dir $(call _i_dir,$(1)),$(2))),\
                            $(call _o_dir,$(1))/$(2) $(call _i_dir,$(1))/$(2).__tubs_install_flags | $(patsubst %/,%,$(dir $(call _i_dir,$(1))/$(2))))\
                            $(if $(BOARD_IP),$(if $(call seq,$(call _i_root_dir,$(1))/$(2),$(TUBS_BOARD_SSH_KEY)),,$(TUBS_BOARD_SSH_KEY)))
	$$(call cmd,cp,$(1),$$@,$$<)
	$$(call _create_install_info,$(1),$$@)
	$(if $(call get_target_deps_output_paths,$(1),_i_dir),$$(call cmd,objcopy,$(1),$$@,--strip-unneeded,test -f $$@))
	$(if $(call get_target_deps_output_paths,$(1),_i_dir),$$(call cmd,patchelf,$(1),$$@,,,$(if $(call get_target_deps_output_rpaths,$(1),ar),--set-rpath $(subst $(space),:,$(strip $(call get_target_deps_output_rpaths,$(1),ar))),--remove-rpath)))
	$(if $(BOARD_IP),$$(call _install_to_target,$(1),$$@))

$(call _i_dir,$(1))/$(2).__tubs_install_flags: .force | $(patsubst %/,%,$(dir $(call _i_dir,$(1))/$(2)))
	$$(call _create_install_flags,$(1),$$@)

endef

define _install_root_mkdir
$(call _ir_to_dir,$(1))/$(2): | $(call _ir_to_dir,$(1))
	$$(call cmd,mkdirp,$(1),$$(@),,,test ! -e $$(@))
endef

# $1 = NOT target-name. Takes name of a variable containing install_root info
# creates rules for makaing directories in the install directory for the target
# this is seperate from setup_output_dirs as it supposed to behave differently
# the entire install_root path is created with '-p' option. And we only care
# that the top of install_root is created.
#$(call ar,$(call _ir_to_dir,$(1))/bin $(call _ir_to_dir,$(1))/lib: | $(call _ir_to_dir,$(1)))
#	$$(call cmd,mkdir,$(1),$$(@))
define setup_install_root
$(__rul_tr1)
$(call _ir_to_dir,$(1)):
	$$(call cmd,mkdirp,$(1),$$(@),,,test ! -e $$(@))

$(if $(call defined,_extra_inst_seen,$(call _ir_to_dir,$(1))_lib),,$(eval $(call _install_root_mkdir,$(1),lib)))
$(if $(call defined,_extra_inst_seen,$(call _ir_to_dir,$(1))_bin),,$(eval $(call _install_root_mkdir,$(1),bin)))
endef

###############################################################################
# Support for integrating external projects
###############################################################################

# This setups the dependency between all of the patch stamp to each other!
# The patch list is assumed to be in order that is is going to be applied, so going
# back from the list, the penultimate word in the list becames the dependend on the last
# item in the list. Then the last item in the list is removed and the function
# recursively calls its self.
#
# $1 = target-name
# $2 = List of remaining patch files
define _setup_patch_dep_rules
ifneq "$(strip $(firstword $(2)))" "$$(strip $(lastword $(2)))"
$(if $(strip $(2)),$(call _setup_patch_dep_rules,$(1),$(patsubst %$(lastword $(2)),%,$(2))))
$(call _stamp_patch,$(1),$(2)): | $(call _stamp_patch,$(1),$(patsubst %$(lastword $(2)),%,$(2)))
endif
endef

# 1. patch-stamp: patch-file | unpacked_source_dir
# $1 = target-name
# $2 = Patch file
define _setup_patch_rules
$(call _stamp_patch,$(1),$(2)): $(2) | $(call _stamp_unpacked,$(1))
	$$(call cmd,patch,$(1),$(2))
	$(Q)$$(TOUCH) $$@
endef

# $1 = target-name
# Setup patch rules
define _setup_patches
$(eval $(call _setup_patch_dep_rules,$(1),$($(1)_patch_files)))
$(foreach _f,$($(1)_patch_files),$(eval $(call _setup_patch_rules,$(1),$(_f))))
endef

# $1 = target-name
# $2 = output produced by the target
# this creates the following rules:
# 1. target-path: install_dir/output
# 2. install_dir/output: output_dir/output
# 3. output_dir/output: output_dir/work_dir patch_stamps
#
# Step #3 is is only if $(target-name)_work_dir is defined
define _setup_external_outputs
$($(1)_path)/install: $(call _i_dir,$(1))/$(2)
$($(1)_path): $(call _o_dir,$(1))/$(2)
$(call _o_dir,$(1))/$(2): $(call _stamp_built,$(1))
$(call _i_root_dir,$(1))/$(2): $(call _stamp_installed,$(1)) $(call _o_dir,$(1))/$(2)

$(call _o_dir,$(1))/$(2): | $(call for_each_targets_stamped_deps,$(1),_stamp_installed)

$(if $(filter-out $($(1)_extra_files),$(2)),$(call setup_outputs_install,$(1),$(2)))
endef

# add depency on unpacked stamp only if there is a archive/or source
# dir exists
define _copy_external_source_dir
$(call _stamp_patched,$(1)): $(call _stamp_unpacked,$(1))

$(call _stamp_unpacked,$(1)): | $(call _ud_dir,$(1))
	$$(call cmd,touch,$(1),$$@)

$(call _ud_dir,$(1)): | $(call _w_dir,$(1))
	$$(call cmd,rsync,$(1),$(call _ud_dir,$(1)),$(call _s_dir,$(1))/$($(1)_source_dir)/)

endef

# $1 = target-name
define _setup_work_dir
$(if $(sne $(call _w_dir,$(1)),$(call _o_dir,$(1))),$(call _w_dir,$(1)):$(call _o_dir,$(1)))
$(call _w_dir,$(1)): | $(patsubst %/,%,$(dir $(call _w_dir,$(1))))
	$$(call cmd,mkdirp,$(1),$$(@),,,test ! -e $$(@))

$(call _o_dir,$(1)): |$(patsubst %/,%,$(dir $(call _o_dir,$(1))))
	$$(call cmd,mkdirp,$(1),$$(@),,,test ! -e $$(@))

endef


define _source_archive_download
$(call ar,$(TUBS_ARCHIVE_DIR)/$($(1)_source_archive): | $(TUBS_ARCHIVE_DIR))
	$$(call cmd,download,$(1),$$@,$($(1)_url))

endef

define _source_archive_download_nourl
$(call ar,$(TUBS_ARCHIVE_DIR)/$($(1)_source_archive):)
	$$(call cmd,echo,$(1),No download URL defined for $($(1)_source_archive) and file does not exist in $(TUBS_ARCHIVE_DIR))
	$$(call cmd,echo,$(1),Define $($(1)_name)_url in $(call _target_to_makefile,$(1)) or set environment variable TUBS_ARCHIVE_DIR to point to directory containing the archive)
	$(Q)false
endef

# $1 = target-name
define _source_archive_rules
$(call _stamp_patched,$(1)): $(call _stamp_unpacked,$(1))
$(call _stamp_unpacked,$(1)) $(if $($(1)_unpacked_dir),$(call _w_dir,$(1))/$($(1)_unpacked_dir)): $(TUBS_ARCHIVE_DIR)/$($(1)_source_archive).valid | $(call _w_dir,$(1))
	$$(call cmd,unpack,$(1),$(call _w_dir,$(1)),$$(TUBS_ARCHIVE_DIR)/$$($(1)_source_archive))
	$$(call cmd,touch,$(1),$$@)

$(TUBS_ARCHIVE_DIR)/$($(1)_source_archive).valid: $(TUBS_ARCHIVE_DIR)/$($(1)_source_archive)
	$$(call cmd,rm,$(1),$$@)
	$$(call cmd,hash,$(1),$$<,,,$($(1)_hash),$($(1)_hash_type))
	$$(call cmd,touch,$(1),$$@)

$(if ($($(1)_url)),$(eval $(call _source_archive_download,$(1))),$(eval $(call _source_archive_download_nourl,$(1))))

endef

# $1 = target-name
# 1. parent-path: target-path
# 2. target-work-dir: target-patches-stamps unpack-stamp
# 3. unpack-stamp: archive | output-dir
#
# 2 & 3 only used when there is an archive file that will be used for the target's source
# 
# Setup a target that uses an external build system
define setup_externals
$(if $(and $($(1)_source_archive),$($(1)_source_dir)),$(error $($(1)_path) has both source_dir and source_archive set. Pick one))

$($($(1)_parent)_path): $($(1)_path)
$($($(1)_parent)_path)/install: $($(1)_path)/install
$(eval $(call _setup_extras,$(1)))

$(call _stamp_installed,$(1)): $(call _stamp_built,$(1)) | $(call _w_dir,$(1))
	$$(call cmd,touch,$(1),$$@)

$(call _stamp_built,$(1)): $(call _stamp_configured,$(1)) | $(call _w_dir,$(1))
	$$(call ext_cmd_build_$$($(1)_ext_build),$(1))
	$$(call ext_cmd_install_$$($(1)_ext_build),$(1))
	$$(call cmd,touch,$(1),$$@)


$(call _stamp_configured,$(1)): | $(call for_each_targets_stamped_deps,$(1),_stamp_installed) $(TUBS_TOOLCHAIN_DEP)
$(call _stamp_configured,$(1)): $(call _stamp_patched,$(1)) $(call _stamp_configured,$(1)).flags | $(call _w_dir,$(1))
	$$(call ext_cmd_configure_$$($(1)_ext_build),$(1))
	$$(call cmd,touch,$(1),$$@)

$(call _stamp_configured,$(1)).flags: .force | $(patsubst %/,%,$(dir $(call _stamp_configured,$(1))))
	$$(call _check_and_store_flags,$(1),$$@,$$($(1)_conf_options))

$(call _stamp_patched,$(1)): $(foreach _f,$($(1)_patch_files),$(call _stamp_patch,$(1),$(_f))) | $(call _w_dir,$(1))
	$$(call cmd,touch,$(1),$$@)

$(if $($(1)_source_archive),$(eval $(call _source_archive_rules,$(1))))

$(if $($(1)_source_dir),$(eval $(call _copy_external_source_dir,$(1))))
$(if $($(1)_work_dir),$(eval $(call _setup_work_dir,$(1))))

$(eval $(call _setup_patches,$(1)))
$(foreach _o,$($(1)_outputs),$(eval $(call _setup_external_outputs,$(1),$(_o))))
$(foreach _o,$($(1)_extra_files),$(eval $(call _i_root_dir,$(1))/$(_o): $(call _stamp_installed,$(1))))
$(foreach _o,$($(1)_extra_files),$(eval $(call _o_dir,$(1))/$(_o): $(call _stamp_installed,$(1))))

$(eval $(call _ext_build_cmd,$(1),setup))
endef


###############################################################################
# Support for building apps/libraries
###############################################################################

# $1 target
# $2 = source file
# $3 = object file
# returns flags that should be used to stored into or compared against contents
# in a.d.flag file.
# The file itself contains one line, the line is the compiler flags that
# are being used to compile the object. Without the -M related flags
_get_dep_compile_flags = $(call cmd_compile,$(1),$(3),$(2),$(TRUE))

# $1 target
# this is like _get_dep_compile_flags but for linker
_get_dep_link_flags = $(call flags_link_$($(1)_link_type),$(1))

# $1 = target-name
# $2 = source file
# $3 = object file
# 1. target-output-dir/object: target-source-dir/source target-output-dir/object.flags | dir(target-output-dir/object)
# 2. target-output-dir/object: | target/installed.stamp
# 3. target-output/object.flags: .force
#
# for #2  add installed.stamp files for any target that we depend on that produces them.
# Make it order only since we only want to depend on it being installed, this not the 
# installation being updated. Do this to prevent the object file from being built until
# the output is installed, but once it gets installed, the .d file will be generated
# during compilation and the object will now depend on proper headers. Magic!
# The final rule provides the recipe for the update the object flags
define _setup_object
$(call _o_dir,$(1))/$(3): $(call _s_dir,$(1))/$(2) $(call _o_dir,$(1))/$(3).d.flags | $(patsubst %/,%,$(dir $(call _o_dir,$(1))/$(2))) $(TUBS_TOOLCHAIN_DEP)
	$$(call cmd,compile,$(1),$$(call _o_dir,$(1))/$(3),$$(call _s_dir,$(1))/$(2))

$(call _o_dir,$(1))/$(3): | $(call for_each_targets_stamped_deps,$(1),_stamp_installed)

$(call _o_dir,$(1))/$(3).d.flags: .force | $(patsubst %/,%,$(dir $(call _o_dir,$(1))/$(2)))
	$$(call _check_and_store_flags,$(1),$$@,$$(call _get_dep_compile_flags,$(1),$(call _s_dir,$(1))/$(2),$(call _o_dir,$(1))/$(3)))

endef

# $1 = target-name
# $2 = output produces by the target
# 
# 2. Setup dependencies between the target's build output and it's object's in the build directory
# 3. Create object build rules
# 4. Creat object compilation flags
#
define _setup_tubs_outputs
$(eval $(call setup_outputs_install,$(1),$(2)))
$(foreach _s,$($(1)_sources),$(eval $(call _setup_object,$(1),$(_s),$(call _source_to_object,$(1),$(_s)))))

$(call _o_dir,$(1))/$(2): $(call _o_dir,$(1))/$(2).d.flags $(strip $(call _get_objects,$(1)) $(call get_target_deps_as_libs,$(1),_o_dir)) | $(call _o_dir,$(1)) $(TUBS_TOOLCHAIN_DEP)
	$$(call cmd,link,$(1),$$(call _o_dir,$(1))/$(2),,,$$(call _get_objects,$(1)))

$(call _o_dir,$(1))/$(2): | $(call for_each_targets_stamped_deps,$(1),_stamp_installed)
$(call _o_dir,$(1))/$(2).d.flags: .force | $(patsubst %/,%,$(dir $(call _o_dir,$(1))/$(2)))
	$$(if $$(wildcard $$(dir $$@)),$$(if $$(findstring $$(strip $$(if $$(wildcard $$@),$$(file <$$@))),$$(call _get_dep_link_flags,$(1))),,$(call cmd,echo,$(1),Updating $$@)$$(file >$$@,$$(strip $$(call _get_dep_link_flags,$(1))))))
endef

define _setup_extra_ouputs
$(call _o_dir,$(1))/$(2): $(call _s_dir,$(1))/$(2)
endef

# $1 = target-name
# 1. parent-path: target-path
# Setup targets that are built using the tubs build system (apps/libraries)
define setup_tubs_target
$($($(1)_parent)_path): $($(1)_path)
$(eval $(call _setup_extras,$(1)))

$(foreach _f,$($(1)_extra_files),$(eval $(call _setup_extra_ouputs,$(1),$(_f))))

$($(1)_path): $(foreach _o,$(call _get_build_outputs,$(1)),$(call _o_dir,$(1))/$(_o))
$($(1)_path): $(foreach _o,$(call _get_extra_outputs,$(1)),$(call _o_dir,$(1))/$(_o))
$(foreach _o,$(call _get_build_outputs,$(1)),$(eval $(call _setup_tubs_outputs,$(1),$(_o))))
endef

# $1 = target-name
# setup additional dependencies based on target's libraries.
# parses the $($(target-name)_libraries) extract evertyhing that doesn't
# start with -l and try to resolve based on libname.so or full/relative path
# to a location that produces a .so
define resolve_libraries
$(eval $(1)_tubs_libraries := $(strip $(sort $($(1)_tubs_libraries) $(foreach _l,$(filter-out -l%,$($(1)_libraries)),$(call find_tubs_target_with_libname,$(1),$(_l),$($(1)_name)_libraries)))))
$(eval $(1)_tubs_libraries_only := $(strip $(sort $($(1)_tubs_libraries_only) $(foreach _l,$(filter-out -l%,$($(1)_libraries_only)),$(call find_tubs_target_with_libname,$(1),$(_l),$($(1)_name)_libraries_only)))))
$(if $(strip $($(1)_tubs_libraries)$($(1)_tubs_libraries_only)),\
    $(foreach _d,$(strip $($(1)_tubs_libraries) $($(1)_tubs_libraries_only)),\
        $(if $(call _get_build_outputs,$(_d)),,\
            $(error $(if $(call _s_dir,$(1)),$(call _s_dir,$(1))/)Makefile: '$($(1)_name)': depends on '$($(_d)_name)' but it doesn't produce any build outputs))))
endef

# $1 = target name
# Like resolve_libraries but for _includes
define resolve_includes
$(eval $(1)_tubs_includes := $(strip $(sort $($(1)_tubs_includes) $(foreach _l,$(filter-out -I%,$($(1)_includes)),$(call find_tubs_target_with_libname,$(1),$(_l),$($(1)_name)_includes)))))
$(if $($(1)_tubs_includes),\
    $(foreach _f,$(addsuffix /*.h,$(filter $(app_targets),$(lib_targets),$(foreach _i,$($(1)_tubs_includes),$(call _s_dir,$(_i))))),\
        $(if $(wildcard $(_f)),,\
        $(error $($(1)_dir)/Makefile: '$($(1)_name)_includes': one of the entries resolves to a directory '$(patsubst %/*.h,%,$(_f))' that doesn't contain any headers))))
endef

# using "associative lists" here

# for each dep
#   complain if dep is in seen list
#   add dep to seen list and set target where it was seen
# recurse with dep

# $1 = target-name
# $2 = libraries,includes
define check_for_dup_deps
$(eval __cfpd := )
$(eval __cfpd_chain := )
$(eval __cfpd_stack := )
$(eval $(call clear,__seen_list))
$(eval $(foreach _di,$(call all_targets_deps,$(1)),\
    $(eval __cfpd := )
    $(eval __cfpd_stack := $(call push,$(__cfpd_stack),$(_di)))\
    $(eval $(call _check_for_dup,$(1),$(2),$(1),$(_di)))\
    $(eval __cfpd_stack := $(call pop,$(__cfpd_stack)))\
    ))
endef
#$(eval $(call clear,__seen_list))

_describe_dep = $(1):'$($(call get,__seen_list,$(4))_name)_$(2)' in '$(call _target_to_makefile,$(call get,__seen_list,$(4)))' contains '$(call _rd_get,$(call get,__seen_list,$(4)),$(4))' and '$($(3)_name)_$(2)' in '$(call _target_to_makefile,$(3))' contains '$(call _rd_get,$(3),$(4))' both resolve to the same target"

# 12, at this  point I really should just import gmsl
# the z infront in not a typo, it's there to disable the depth check
# if you suspect the problem is because the check_for_dup code, remove
# the z and try to decode the error output.
__cfp_depth = zx x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x

# $1 = target-name, where check started from
# $3 = target-name of dup that we are checking on first call $1 == $3
# $4 = dupe that we are checking
define _check_for_dup
$(if $(4),\
    $(eval __cfpd += x)$(eval __cfpd_chain += $($(3)_path):$($(4)_path))\
    $(eval $(call set,__seen_list,$(4),$(3)))\
    $(foreach __d,$(call l_targets_deps,$(4)),\
        $(if $(findstring :$(__d):,$(__cfpd_stack):),\
            $(if $(findstring :$(3):,$(__cfpd_stack):),\
                $(warning circuler reference $($(__d)_path) and $($(4)_path) remove one of the refences),\
                $(warning duplicate reference to ---$(call _rd_get,$(3),$(__d))--- / $($(__d)_path) in $($(call get,__seen_list,$(__d))_path) and $($(4)_path) remove one of the refences)),\
            $(eval $(call set,__seen_list,$(__d),$(3)))\
            $(eval __cfpd_stack := $(call push,$(__cfpd_stack),$(__d)))\
            $(eval $(call _check_for_dup,$(1),$(2),$(4),$(__d)))\
            $(eval __cfpd_stack := $(call pop,$(__cfpd_stack)))\
        )))
endef

###############################################################################
# Pull in files from tubs.d
#
# The loading of sub-makefiles happens in three steps.
# First one is here, at this point everything in the file gets it's
# first expansion. Evertying in the makefile in slurped up into our namespace
# The included file should add a simple unique name that identifies itself into the
# _TUBS_LOADED_SUB_MAKES variables. (let's call it module prefix)
# The second time is when the _tubs_load_head variables are expanded
# And finally when _tubs_load_tail variables are expanded
#
# those were expansion forced on you by Make and inclusion of different
# parts of the sub-makefile into TUBS hook. Another expansion would happen
# of the various ext_cmd_xxx variables during the appropirate build steps.
#
# The sub-make can define a variables with that name prefix to them, which will 
# be used by TUBS as needed.
# Example of this is the vscode.mk, it adds 'vscode' word during load and
# defines vscode_tubs_load_tail variable. This variable will be expanded and
# $(evaled ) later.
#
# There are two naming conventions expected from the sub-makefile for variables,
# that it defines, for them to be automatically used by TUBS.
# The first has the form of module_prefix_tubs_xxx, and they are used to add
# to core functionality of TUBS. 
# the second set are the ext_cmd_xxx_module_prefix variable. These are used to
# add extra commands to execute during different staged of the build.
#
# Generally, these variable should only create new variables when they are expanded.
# The exception is _tubs_setup_custom, it should be creating dependency relationships
# between rules and can add recipes for the rules. It is preferred that $(call cmd,xxx)
# style commands be used in recipes. You really want the world to know
# that you are doing something, plus cmd, cmd_ext will make sure that the execution 
# environment is sane.
#
# 
#   _tubs_load_head      - expanded before any Makefile has been loaded
#   _tubs_load_tail      - expanded after everything has been processed and generated
#   _tubs_include_custom - expanded whenever include_subdir find a variable
#                          in the Makefile it is currently handling that matches
#                          the name that is provided in _TUBS_LOADED_SUB_MAKES.
#                          The variable get the same arguments as other include_
#                          variables when it is expanded.
#   _tubs_setup_custom   - expanded after all of the internal setup_xxx variables
#                          have been expanded. Used to setup rules to handle the
#                          new target that was defined because of _tubs_include_custom
#   _tubs_help           - expanded when help is generated. Each line should 
#                          with a tab followed by @$(ECHO) then a double quoted
#                          string with the text to output
# 
# both _tubs_load_head and _tubs_load_tail are invoked during the first pass
# expansion, right after everything has finished being included by make. 
# at this stage none of the rules have been setup yet, so other rules
# shouldn't be set here. use the _tubs_setup_custom for that.
###############################################################################
ifneq ($(wildcard $(T)/tubs.d/*.mk),)
$(wildcard $(T)/tubs.d/*.mk): ;
include $(wildcard $(T)/tubs.d/*.mk)
endif

$(foreach _f,$(_TUBS_LOADED_SUB_MAKES),$(eval $(call $($(_f)_tubs_load_head))))
###############################################################################
###############################################################################
# This is where processing of everything starts                               #
###############################################################################
###############################################################################
# start evaluating the top-level Makefile
$(call _tubs_log,Loading Makefiles)
$(eval $(call include_subdir,all,$(patsubst %/,%,$(s))))

ifeq "$(flavor PROJECT_NAME)" "undefined"
$(error Define PROJECT_NAME in the top-level Makefile)
endif

ifeq ($(skip_libraries_resolve),n)
$(call _tubs_log,Resolving library dependencies)
$(eval $(call for_all_vars_breadth,all,libraries,resolve_libraries))
endif
ifeq ($(skip_includes_resolve),n)
$(call _tubs_log,Resolving include dependencies)
$(eval $(call for_all_vars_breadth,all,includes,resolve_includes))
endif
ifeq ($(skip_dupe_check),n)
$(call _tubs_log,Checking for duplicate dependencies)
$(eval $(call for_each_target_breadth,all,check_for_dup_deps,includes))
$(eval $(call clear,__seen_list))
endif

$(call _tubs_log,Loading post-makefiles)
# process post-makefiles
$(foreach _t,$(post_makefile_targets),$(eval $(call include_post_makefile,$(_t))))

$(foreach _t,$(TUBS_GROUP_TO_ID_MAP),\
    $(if $(findstring :,$(_t)),\
        $(call set,_group_to_id_map,$(word 1,$(subst :, ,$(_t))),$(word 2,$(subst :, ,$(_t)))),\
        $(error TUBS_GROUP_TO_ID_MAP contains a mapping that I don't know how to handle '$(_t)')))
        
$(foreach _t,$(TUBS_USER_TO_ID_MAP),\
    $(if $(findstring :,$(_t)),\
        $(call set,_user_to_id_map,$(word 1,$(subst :, ,$(_t))),$(word 2,$(subst :, ,$(_t)))),\
        $(error TUBS_USER_TO_ID_MAP contains a mapping that I don't know how to handle '$(_t)')))

###############################################################################
# Start of rules 
###############################################################################
$(call _tubs_log,Generating subdir rules)
# create all sub-directory targets
$(foreach _t,$(wordlist 2,$(words $(subdir_targets)),$(subdir_targets)),$(eval $(call setup_subdir,$(_t))))
$(call _tubs_log,Generating externals rules)
$(foreach _t,$(external_targets),$(eval $(call setup_externals,$(_t))))
$(call _tubs_log,Generating application rules)
$(foreach _t,$(strip $(app_targets) $(lib_targets)),$(eval $(call setup_tubs_target,$(_t))))
$(call _tubs_log,Generating installation rules)
$(foreach _t,$(install_roots),$(eval $(call setup_install_root,$(_t))))
$(call _tubs_log,Processing custom rules)
$(foreach _t,$(_TUBS_LOADED_SUB_MAKES),$(eval $(call $(_t)_tubs_setup_custom,$(_t))))
$(call _tubs_log,Processing custom rules Done)


$(sort $(addprefix $(o),$(all_extra_output_dirs))):
	$(call cmd,mkdirp,all,$(@),,,test ! -e $(@))

# create rules to mkdir the top of output and install tree
$(patsubst %/,%,$(o)):
	$(call cmd,mkdirp,all,$(@),,,test ! -e $(@))

$(TUBS_ARCHIVE_DIR):
	$(call cmd,mkdir,all,$(@),,,test ! -e $(@))

$(TUBS_TOOLCHAINS_DIR):
	$(call cmd,mkdir,all,$(@),,,test ! -e $(@))

###############################################################################
# Utility rules
###############################################################################
define print_all_targets
$(Q)$(ECHO) "$2$($1_path)   $(if $(filter $(1),$(app_targets)),[app],$(if $(filter $(1),$(lib_targets)),[lib],$(if $(filter $(1),$(external_targets)),[ext],$(space)$(space))))"
$(foreach _t,$($1_targets),$(call print_all_targets,$(_t),$2$(space)$(space)))
endef

.PHONY: targets
targets:
	$(foreach _t,all,$(call print_all_targets,$(_t)))

tubs-var-%:
	@echo "$($*)"

tubs-target-var-%:
	@echo "$(foreach _t,$(call get_all_var_contents_shell,$(*)),$(_t))"

tubs-path-to-target-%:
	echo "$(if $($(call path_to_target,$(*))),$(call path_to_target,$(*)))"

tubs-target-to-src-%:
	echo "$(call _s_dir,$(*))"

tubs-target-to-output-%:
	echo "$(call _o_dir,$(*))"

tubs-target-find-output-%:
	echo "$(call find_target_with_output)

###############################################################################
# setup handling make being invoked to perform a target in a sub-directory
# ie. 'make src/app_dir/clean'
###############################################################################

# $1 = target-name
# Generate the commands needed to clean the target's outputs
define _gen_clean_commands
$(if $(call _get_build_outputs,$(1)),$(call cmd,rm,$(1),$(addprefix $(call _o_dir,$(1))/,$(call _get_build_outputs,$(1)))))
$(if $($(1)_clean_files),$(call cmd,rm,$(1),$($(1)_clean_files)))
$(if $($(1)_extra_dirs)$($(1)_clean_dirs),$(call cmd,rmdir,$(1),$(foreach _o,$($(1)_extra_dirs)$($(1)_clean_dirs),$(call _o_dir,$(1))/$(_o))))
endef
    
# $1 target-name
gen_clean_commands = $(call for_each_target_depth,$(1),_gen_clean_commands,$(1))

# only generate commands for targets that we link/compile ourselves
define _gen_cleandep_commands
$(if $($(1)_link_type),$(call cmd,rm,$(1),$(addsuffix .d.flags,$(strip $(call _o_dir,$(1))/$(call _get_build_outputs,$(1)) $(call _get_objects,$(1))))))
$(if $($(1)_link_type),$(call cmd,rm,$(1),$(addsuffix .d,$(strip $(call _get_objects,$(1))))))
endef

# $1 = target-name
gen_cleandeps_commands = $(call for_each_target_depth,$(1),_gen_cleandep_commands,$(1))

# $1 target-name
define clean_sub_rules
$(call _o_dir_rel,$(1))/clean $($(1)_path)/clean: $(foreach _t,$(filter $(1)%,$(post_makefile_targets)),$($(_t)_post_clean_rules))
	$$(call gen_clean_commands,$(1))
	$$(foreach _st,$$(call for_all_stamped_targets,$(1),_expand_var),$$(call _ext_build_cmd,$$(_st),clean))
endef

define distclean_sub_rules
$(call _o_dir_rel,$(1))/distclean $($(1)_path)/distclean:
	$$(call gen_clean_commands,$(1))
	$$(foreach _st,$$(call for_all_stamped_targets,$(1),_expand_var),$$(call _ext_build_cmd,$$(_st),distclean))
	$$(call gen_cleandeps_commands,$(1))
	$(foreach _f,$(_TUBS_LOADED_SUB_MAKES),$(eval $(call $($(_f)_tubs_distclean))))
endef

define cleandeps_sub_rules
$(call _o_dir_rel,$(1))/cleandeps $($(1)_path)/cleandeps:
	$$(call gen_cleandeps_commands,$(1))
endef

# $1 target-name
define all_sub_rules
$(call _o_dir_rel,$(1))/all $($(1)_path)/all: $($(_target)_path)
	$$(call echo_build_done,all)
endef

define install_sub_rules
$(call _o_dir_rel,$(1))/install $($(1)_path)/install:
	$$(call echo_build_done,all)
endef

# $1 target-name
# Prints the target-name for the path. Used for shell integration
define as_target_sub_rules
$($(1)_path)/as_target $(call _o_dir_rel,$(1))/as_target:
	@echo $(1)

%/as_target: ;
endef

ifneq ($(filter $(addprefix %/,$(_TUBS_ALLOWED_SUBDIR_TARGETS)),$(MAKECMDGOALS)),)
    _build_target := $(notdir $(MAKECMDGOALS))
    _path := $(patsubst %/$(_build_target),%,$(MAKECMDGOALS))
    # handle being launched relative to the source, output, or install directory
    _path_new := $(strip $(if $(filter $(i)%,$(_path)),\
                             $(patsubst $(i)%,%,$(_path)),\
                             $(if $(filter $(output_dir_name)%,$(_path)),\
                                $(patsubst /%,%,$(patsubst $(output_dir_name)%,%,$(_path))),\
                                $(_path))))

    _target_name := $(call path_to_target,$(_path_new))

    ifeq ($(_target_name),all_all)
    _target_name = all
    endif
    _target = $(_target_name)
    #_target = $(if $($(_target_name)),$(_target_name))

# call one of the _sub_rules defined above based on $(_build_target)
$(if $(_target),$(eval $(call $(_build_target)_sub_rules,$(_target))))

else
    .DEFAULT_GOAL = all
endif

# As was mentioned in the comment above, this is not the real 'all' target
# this here is just to print out the build times

.PHONY: .force
.force:

define _include_dep_file
$(1): ;
-include $(1)
endef

# Go through all library and app targets and include .d files
$(foreach _d,$(foreach _t,$(app_targets) $(lib_targets) $(_tua_tests_targets),$(call get_target_source_deps,$(_t))),$(if $(wildcard $(_d)),$(eval $(call _include_dep_file,$(_d)))))

# now load tail variables in our tubs.d makefiles
$(foreach _f,$(_TUBS_LOADED_SUB_MAKES),$(call $($(_f)_tubs_load_tail)))

# setup rules to download toolchain only toolchain's location is in $(s)
ifneq "$(findstring $(s)toolchains,$(TUBS_TOOLCHAIN_PATH))" ""

$(TUBS_TOOLCHAINS_DIR)/.downloaded_$(TUBS_TOOLCHAIN_ARCHIVE): | $(TUBS_TOOLCHAINS_DIR)
	$(call cmd,download,all,$(TUBS_TOOLCHAINS_DIR)/$(TUBS_TOOLCHAIN_ARCHIVE),$(TUBS_TOOLCHAIN_URL))
	$(call cmd,touch,all,$@)

$(TUBS_TOOLCHAINS_DIR)/.setup_$(TUBS_TOOLCHAIN_ARCHIVE): $(TUBS_TOOLCHAINS_DIR)/.downloaded_$(TUBS_TOOLCHAIN_ARCHIVE)
	$(call cmd,unpack_blind,all,$(TUBS_TOOLCHAINS_DIR),$(TUBS_TOOLCHAINS_DIR)/$(TUBS_TOOLCHAIN_ARCHIVE))
	$(call cmd,run,all,Relocating SDK,cd $(TUBS_TOOLCHAIN_PATH) && ./relocate-sdk.sh)
	$(call cmd,touch,all,$@)
	$(call cmd,echo,all,Using downloaded toolchain in $(TUBS_TOOLCHAIN_PATH))

$(CC) $(CXX) $(LD) $(AR) $(AS) $(NM) $(CPP) $(RANLIB) $(READELF) $(STRIP) $(OBJCOPY) $(OBJDUMP) $(PKG_CONFIG): $(TUBS_TOOLCHAINS_DIR)/.setup_$(TUBS_TOOLCHAIN_ARCHIVE)

download_toolchain: $(TUBS_TOOLCHAINS_DIR)/.setup_$(TUBS_TOOLCHAIN_ARCHIVE)

endif
