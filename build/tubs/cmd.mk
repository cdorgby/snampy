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

# TBD
__magic = F540106F001E3ECBA5CF8973B3D9BFC5

# $1 = command to run
# $2 = if set must execute to bash's true for $1 to be executed
# $3 = if set /usr/bin/env will be used to clear/update the environment
# This wrapper is responsible for adding some extra crud around the
# command-line to execute.
# if $2 is set, this will be executed as test and it must return bash
# equivelant of 'true' for the command in $1 to be executed. This is
# use for testing if a file exists to prevent errors on rm
# $3 is used to setup the commands enviroment. If it's set then
# /usr/bin/env will be used to execute passed in command. This won't clear
# the the enviroment, value in $3 must contain -c or - for that to happen.
__cmd = $(if $(2),$(2) && ( $(if $(3),$(ENV) $(3) $(SH) "$(1)",$(1)) ) || true,$(if $(3),$(ENV) $(3) $(SH) "$(1)",$(1)))

# $1 = target-name
# $2 = cmd-name
# $3 = prety-print string
# $4 = command to execute arguments and all
# $5 = A shell command that must exit with 1 for command in $4 to run.
# $6 = if provided, env will be used to launch the command with $6 being the options
# passed to env as is. The command will looksomething like this /usr/bin/evn $(6) "$(5) && $(4) || true"
# Warning!, NB. ETC!: make sure that there is a space between $(call __cmd,$(4),$(5)) and the ')' following it 
# in the line with set -x.
# very important. THAT SPACE IS NOT EXTRA.
# This wrapper is responsible of what is displayed on the screen when a command
# is ran.
# If in silent mode, don't output anything to the screen
# If Q=@ output the command line and it's output to the log file
# If Q= output the command line and it's output to the screen
# so if make is ran like so 'Q= make -s ...' then no output will go to the screen
# and nothing will be written to the log file
_cmd_wrap        = $(__cmd_tr5)$(if $(Q),$(Q)$(if $(SILENT),,$(ECHO) "[`printf "%-12s" $2`] $(3)";) \
                       (set -x; $(call __cmd,$(4),$(5),$(6)) ) \
                       $(if $(Q),1>>$(log_file) 2>>$(log_file) || (tail -n 20 $(log_file); echo "**** Build failed possible source of failure: $(if $($(1)_dir),$($(1)_dir)/)Makefile, check $(log_file)"; exit 1; ));,\
                       $(call __cmd,$(4),$(5),$(6)))

# $1 = cmd to execute
# $2 = target-name
# $3 = what we will be acting on, usually output file or target file $(2) to subcommand
# $4 = should be the input file for single source command, leave empty for, 
#      when there are mupltiple value and isntead set $6. $(3) to subcommand
# $5 = passed as fifth argument to cmd_wrap (if not empty the command must exit with status 0 for the command to run)
# $6 = pass value here instead of $4 if you don't want it included in the screen
#      output during a build with Q=@
# $7-$9 passed as $4-$6 to cmd_$(1)
# And this wrapper is used to setup the a consistent visual appearance of all
# of the commands when they are displayed to the screen.
cmd = $(__cmd_tr7)$(call _cmd_wrap,$(2),$(if $(call cmd_$(1)),$(call cmd_$(1),,$(3),$(4)$(6),$(7),$(8),$(9)),$(1)),$(if $(4),$(4) -> )$(3),$(call cmd_$(1),$(2),$(3),$(4)$(6),$(7),$(8),$(9)),$(5),$(10))

# This wrapper is like $(_cmd) with additional function of setting up a
# custom shell enviroment to execute a command.
#
# The following shell variables will get modified/set
#   PATH add host_tools/bin to front of path
#   PKG_CONFIG_PATH set to toolchain's lib/pkgconfig, and one relative to every install_root
#   Copy toolchain environment variables into regular Make equivalents
#   Copy values from $(1)_build_env and $($(1)_parent)_build_env variables
#
# That should be enough to get most autoconf & cmake based build system working
# automagically
#
# pass all argument to _cmd as is, except $5, add that to the front of generated
# env list. This allows passing of command line and '-' to env.
cmd_ext = $(__cmd_tr3)$(call cmd,$(1),$(2),$(3),$(4),$(5),$(6),$(7),$(8),$(9),$(strip $(10) $(call _ext_cmd_env,$(2))))

__add_pkgcfg = $(call _o_dir,$(1))/lib/pkgconfig

_ext_cmd_env =- $(strip PATH="$(s)host_tools/bin:$(TUBS_TOOLCHAIN_PATH)/bin:$(PATH)" \
                    CC_FOR_BUILD="gcc" \
                    CXX_FOR_BULID="c++" \
                    LD_FOR_BUILD="ld" \
                    AS_FOR_BUILD="as" \
                    CFLAGS="$(CFLAGS)" \
                    CPPFLAGS="$(CPPFLAGS)" \
                    CXXFLAGS="$(CXXFLAGS)" \
                    LDFLAGS="$(LDFLAGS)" \
                    AR=$(AR) \
                    AS=$(AS) \
                    LD=$(LD) \
                    NM=$(NM) \
                    CC=$(CC) \
                    GCC=$(GCC) \
                    CPP=$(CPP) \
                    CXX=$(CXX) \
                    RANLIB=$(RANLIB) \
                    READELF=$(READELF) \
                    STRIP=$(STRIP) \
                    OBJCOPY=$(OBJCOPY) \
                    OBJDUMP=$(OBJDUMP) \
                    PKG_CONFIG="/usr/bin/pkg-config" \
                    PKG_CONFIG_PATH="$(subst $(space),:,$(sort $(call for_each_targets_stamped_deps,$(1),__add_pkgcfg) $(TUBS_PKG_CONFIG_PATH)))" \
                    $($(1)_cmd_env))

# $1 = target-name
# $3 = command to run
# run an abitrary command
cmd_run = $(__cmd_tr2)$(if $(1),$(3))

# All of the cmd_xxx variable expect at least two argument, but can
# be called with no argumenets in that case it should output what
# should be displayed at the start of the line that the command is
# used on. If they don't return anything in the second case the name
# of the variable with cmd_ prefix removed is used.
#
# most implementation should be similar to this:
#   cmd_mkdir = $(if $(1),$(MKDIR) $(2) ...,mkdir)
#
#

# $1 = target-name
# $2 = list of things to remove
cmd_rm              = $(__cmd_tr2)$(if $(1),$(RM) -fd $(2))
cmd_rmdir           = $(__cmd_tr2)$(if $(1),$(RM) -frd $(2))


# $1 = target-name
# $2 = direcotry
cmd_mkdir           = $(__cmd_tr2)$(if $(1),$(MKDIR) $(2))

cmd_mkdirp          = $(__cmd_tr2)$(if $(1),$(MKDIR) -p $(2))

# $1 = target-name
# $2 = to
# $3 = from
cmd_cp              = $(__cmd_tr3)$(if $(1),$(CP) -d $(3) $(2))
cmd_mv              = $(__cmd_tr2)$(if $(1),$(MV) $(3) $(2))

# $1 = target-name
# $2 = text to echo
# $3 = (optional) could be additional text to display or some
# other valid shell syntax like a redirect.
cmd_echo            = $(__cmd_tr3)$(if $(1),$(ECHO) $(2)$(3))

# $1 = target-name
# $2 = file-to process
# $3 = rest of string
# invoke sed to modify $2 in place
cmd_sed             = $(__cmd_tr3)$(if $(1),$(SED) $(3) -i $(2))


# $1 = target-name, passed up to _cmd_wrap
# displays a build done string with run time, usuful for build targets like all
_time_now = $(shell date +%s)
_bd_str = Build Complete in $$(($(_time_now)-$(_TUBS_BUILD_START_TIME))) seconds
echo_build_done = $(if $(1),$(if $(SILENT),,@$(ECHO) $(_bd_str) | tee -a $(log_file)))

# $1 = target-name
# $2 = directory to create
# $3 = archive
cmd_unpack =    $(__cmd_tr3)$(strip $(if $(1),\
                                $(if $(filter %.zip,$(3)),\
                                    unzip $(3) -d $(patsubst %/,%,$(call _w_dir,$(1))/$(4)),\
                                    $(if $(filter %.tar.gz,$(3)),\
                                        gzip -cd $(3),\
                                        $(if $(filter %.tar.bz2,$(3)),\
                                            bzip2 -cd $(3),\
                                            $(if $(filter %.tar.xz,$(3)),\
                                                xz -cd $(3),\
                                                $(if $(filter %.tar,$(3)),$(CAT) $(3))))) | tar $(if $(4),--strip-components=1 )-C $(patsubst %/,%,$(call _w_dir,$(1))/$(4)) -vxf - )))

cmd_unpack_blind =    $(__cmd_tr3)$(strip $(if $(1),\
                                $(if $(filter %.tar.gz,$(3)),\
                                    gzip -cd $(3),\
                                    $(if $(filter %.tar.bz2,$(3)),\
                                        bzip2 -cd $(3),\
                                        $(if $(filter %.tar.xz,$(3)),\
                                            xz -cd $(3),\
                                            $(if $(filter %.tar,$(3)),$(CAT) $(3))))) | tar $(if $(4),--strip-components=1 )-C $(2)/$(4) -vxf - ))

# $1 = target-name
# $2 = patch
cmd_patch           = $(__cmd_tr2)$(if $(1),(cd $(call _ud_dir,$(1)) && $(PATCH) -p1 < $(2) ))

# $1 = target-name
# $2 = target to invoke
# launch another instance of TUBS build
cmd_make_me        = $(__cmd_tr3)$(if $(1),$(MAKE) -f $(_ME) TARGET=$(TARGET) TUBS_ARCHIVE_DIR=$(TUBS_ARCHIVE_DIR) $(2))
# $1 = target-name
# $2 = directory to invoke make in
# $3 = command line to pass to th external make
cmd_ext_make        = $(__cmd_tr3)$(if $(1),$(ENV) - PATH="$$PATH" $(MAKE) -C $(2) $(3))

# $1 = target-name
# $2 = output file
# $3 = source url
# $4 = extra args to wget
cmd_download        = $(__cmd_tr3)$(if $(1),$(WGET) $(3) -O $(2) $(4))

# $1 = target-name
# $2 = file
# $3 = hash
# $4 = hash-type,
cmd_hash            = $(__cmd_tr4)$(if $(1),$(call cmd_hash_$(4),$(1),$(2),$(3)))
cmd_hash_sha256     = $(__cmd_tr4)$(if $(1),$(ECHO) "$(3) $(tab)$(2)" | $(SHA256) -c -,sha256)
cmd_hash_sha1       = $(__cmd_tr4)$(if $(1),$(ECHO) "$(3) $(tab)$(2)" | $(SHA1) -c -,sha1)

# $1 = target-name
# $2 = path
cmd_touch           = $(__cmd_tr2)$(if $(1),$(TOUCH) $(2))

# $1 = target-name
# $2 = path
# $3 = mode
cmd_chmod           = $(__cmd_tr3)$(if $(1),$(CHMOD) $(3) $(2))

# $1 = target-name
# $2 = file to patch
# $3 = parameters to pass to patchelf
# makesure that the file exists first
cmd_patchelf        = $(__cmd_tr3)$(if $(1),test -f $(2) -a ! -L $(2) && $(PATCHELF) $(3) $(2) || true)

# $1 = target-name
# $2 = diffing to (modified)
# $3 = diffing fromt (original)
# $4 = out filename
cmd_diff = $(__cmd_tr4)$(if $(1),$(DIFF) $(3) $(2) > $(4))

# $1 = target-name
# $2 = path where $3 will rsynced to
# $3 = source path
cmd_rsync = $(__cmd_tr3)$(if $(1),$(strip rsync -a --exclude .svn --exclude .git --exclude .hg \
                                                --exclude .bzr --exclude CVS \
                                                --link-dest=$(3) $(3) $(2)))

# $1 = target-name
# $2 = executable/object path
# $3 = arguments to pass to objcopy
cmd_objcopy = $(__cmd_tr3)$(if $(1),$(OBJCOPY) $(3) $(2),objcopy)

# $1 = target-name
# $2 = object file
# $3 = source file
# $4 = If $(TRUE) use flags_compile_base instead of flags_compile. This is used when .d.flags files
# are generated
cmd_compile_cpp     = $(__cmd_tr3)$(if $(1),$(CXX) $(call $(if $(4),flags_compile_base,flags_compile),$(1),$(3),$(2),cpp) -c $(3) -o $(2),cxx)
cmd_compile_c       = $(__cmd_tr3)$(if $(1),$(CC) $(call $(if $(4),flags_compile_base,flags_compile),$(1),$(3),$(2),c) -c $(3) -o $(2),cc)

# $1 = target-name
# $2 = output name
# $3 = object files
cmd_link_so_cpp     = $(__cmd_tr3)$(if $(1),$(CXX) $(call flags_link,$(1),so_cpp,$(3)) -o $(2),ld cpp so)
cmd_link_app_cpp    = $(__cmd_tr3)$(if $(1),$(CXX) $(call flags_link,$(1),app_cpp,$(3)) -o $(2),ld cpp app)

cmd_link_so_c       = $(__cmd_tr3)$(if $(1),$(CC) $(call flags_link,$(1),so_c,$(3)) -o $(2),ld c so)
cmd_link_app_c      = $(__cmd_tr3)$(if $(1),$(CC) $(call flags_link,$(1),app_c,$(3)) -o $(2),ld c app)

# $1 = target-name
# $2 = object files
# $3 = output name
# based on $(1)_link_type expand appropriate cmd_link_ variable
cmd_link            = $(__cmd_tr3)$(if $(1),$(call cmd_link_$($(1)_link_type),$(1),$(2),$(3)),$(call cmd_link_$($(1)_link_type)))

# $1 = target-name
# $2 = object file
# $3 = source file
cmd_compile  = $(__cmd_tr3)$(if $(1),$(call cmd_compile_$(patsubst .%,%,$(suffix $(3))),$(1),$(2),$(3),$(4)),$(call cmd_compile_$(patsubst .%,%,$(suffix $(3)))))


#######################################################################################
# Compiler/Linker Flag
#######################################################################################
_flags_compile_common   = -Wall -Wno-unused-result -pthread -fPIC -ffile-prefix-map=$(S)=./
_flags_includes_common  = -I$(call _s_dir,$(1))

# $1 = target-name
# this variable expands to all includes directories that are exported (or implicitly generated)
# for each dependency of $1. And the target itself, that's the reason for the extra $(1) in
# the foreach call.
_flags_includes_target  = $(__cmd_tr1)$(strip $(addprefix -I,$(foreach _d,$(1) $(call for_all_targets_deps,$(1)),\
                                                $(if $($(_d)_export_includes),\
                                                    $(addprefix $(if $($(_d)_stamped),\
                                                                    $(call _o_dir,$(_d))/,\
                                                                    $(call _s_dir,$(_d))/),$($(_d)_export_includes)),\
                                                    $(call _s_dir,$(_d)))) $(addprefix  $(call _s_dir,$(1))/,$($(1)_local_includes))))

#_flags_includes_target  = $(strip $(addprefix -I,$(foreach _d,$($(1)_tubs_libraries),$(call _s_dir,$(_d))))\
#                                  $(addprefix -I,$(foreach _d,$($(1)_tubs_includes),$(call _s_dir,$(_d)))))

_flags_defines_common   = -DTUBS_BUILD_TYPE=\"$(TUBS_CONFIG_BUILD_TYPE)\" -DTUBS_BUILD_TARGET=$(1) \
                          -DTUBS_TARGET_VERSION=\"$($(1)_version)\" -DTUBS_TARGET_NAME=\"$($(1)_name)\" \
                          -DTUBS_BUILD_DEBUG=$(if $(TUBS_CONFIG_DEBUG),1,0) -DTUBS_BUILD_RELEASE=$(if $(TUBS_CONFIG_RELEASE),1,0) \
                          -DTUBS_INSTALL_DIR=\"$(patsubst %/,%,$($(1)_install_root))\" -DTUBS_BUILD_BOARD=\"$(BUILD_BOARD)\" \
                          -DTUBS_CONFIG_HW_TARGET=HW_PLATFORM_$(TUBS_CONFIG_HW_TARGET) -DTUBS_BUILD_DIR=\"$(patsubst %/,%,$(o))\" \
                          -DTUBS_ROOT_DIR=\"$(patsubst %/,%,$(S))\" -DTUBS_SOURCE_DIR=\"$(patsubst %/,%,$(call _s_dir,$(1)))\" \
                          -DTUBS_PROJECT_NAME=\"$(PROJECT_NAME)\" -DTUBS_PROJECT_VERSION=\"$(GIT_DESCRIBE)\" \
                           $(TUBS_CPPFLAGS)

_flags_defines          = $(_flags_defines_common) $(DEFINES) $($(1)_defines)

_flags_link_common      = -pthread
_flags_link_target_dirs = $(strip $(addprefix -L,$(sort $(foreach _d,$($(1)_tubs_libraries) $($(1)_tubs_libraries_only),$(call _o_dir,$(_d))))))
_flags_link_target_dirs = $(strip $(addprefix -L,$(sort $(foreach _d,$(call for_all_targets_libraries_deps,$(1)),$(if $($(_d)_export_libraries),$(addprefix $(call _o_dir,$(_d))/,$($(_d)_export_libraries)),$(call _o_dir,$(_d)))))))

_flags_link_target_libs = $(strip $(addprefix -l,$(sort $(foreach _d,$(call for_all_targets_libraries_deps,$(1)),$(patsubst lib%.so,%,$(filter lib%.so,$(patsubst $(addsuffix /%,$($(_d)_export_libraries)),%,$($(_d)_outputs))))))))
_flags_link_system_libs = $(strip $(filter -l%,$($(1)_libraries)) $(TUBS_EXTRA_SYSTEM_LIBS))

__rpath_start           = -Wl,-rpath,
_flags_link_rpaths      = $(if $(call all_targets_libraries_deps,$(1)),$(addprefix $(__rpath_start),$(sort $(foreach _ld,$(call for_all_targets_libraries_deps,$(1)),$(call get_target_rpaths,$(_ld))))))

# $1 target-name
flags_compile_c         = $(strip $(_flags_defines) $(CFLAGS) $(TUBS_CFLAGS) $($(1)_cflags) $(_flags_compile_common) $(_flags_includes_common) $(call _flags_includes_target,$(1)) $($(1)_cppflags))
flags_compile_cpp       = $(strip $(_flags_defines) $(CXXFLAGS) $(TUBS_CXXFLAGS) $($(1)_cxxflags) $(_flags_compile_common) $(_flags_includes_common) $(call _flags_includes_target,$(1)) $($(1)_cppflags))

# $1 = target-name
# $2 = source file
# $3 = object file
# $4 = extension
flags_compile_base      = $(call flags_compile_$(4),$(1))
flags_compile           = $(call flags_compile_base,$(1),$(2),$(3),$(4)) -MMD -MF $(3:.o=.o.d)

# $1 = target-name
# $2 = list of objects
# Warning! Make sure to keep $(_flags_link_common) flags before $(call _flags_link_target_dirs)
# this is to make sure that the later paths are used to resolve any libraries tha are
# provided by TUBS and by the toolchain
flags_link_so_cpp       = $(strip $(LDFLAGS) $(TUBS_LDFLAGS) $($(1)_ldflags) -shared $(_flags_link_common) $(call _flags_link_target_dirs,$(1)) $(call _flags_link_target,$(1)) \
                            -Wl,--start-group $(TUBS_LDFLAGS) $(strip $(2) $(call _flags_link_target_libs,$(1)) $(call _flags_link_system_libs,$(1))) -Wl,--end-group)

flags_link_app_cpp      = $(strip $(LDFLAGS) $(TUBS_LDFLAGS) $($(1)_ldflags) $(_flags_link_common) $(call _flags_link_target_dirs,$(1)) $(call _flags_link_rpaths,$(1))\
                            -Wl,--start-group $(strip $(2) $(call _flags_link_target_libs,$(1)) $(call _flags_link_system_libs,$(1))) -Wl,--end-group)

flags_link_so_c         = $(strip $(LDFLAGS) $(TUBS_LDFLAGS) $($(1)_ldflags) -shared $(_flags_link_common) $(call _flags_link_target_dirs,$(1)) $(call _flags_link_rpaths,$(1))\
                            -Wl,--start-group $(strip $(2) $(call _flags_link_target_libs,$(1)) $(call _flags_link_system_libs,$(1))) -Wl,--end-group)

flags_link_app_c        = $(strip $(LDFLAGS) $(TUBS_LDFLAGS) $($(1)_ldflags) $(_flags_link_common) $(call _flags_link_target_dirs,$(1)) $(call _flags_link_rpaths,$(1))\
                            -Wl,--start-group $(strip $(2) $(call _flags_link_target_libs,$(1)) $(call _flags_link_system_libs,$(1))) -Wl,--end-group)

# $1 = target-name
# $2 = extension/type
# $3 = list of objects
flags_link              = $(call flags_link_$(2),$(1),$(3))
