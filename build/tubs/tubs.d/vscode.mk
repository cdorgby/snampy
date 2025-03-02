
###############################################################################
# visual code support
##############################################################################


# Add to your Visual Code Settings, otherwise VS will eat all of your memory
# on the build machine.

# "C_Cpp.files.exclude": {
#    "**/output.*/**": true,
# },
#" files.watcherExclude": {
#     "**/output.*/**": true
# }
#
# Visual code should have at least the following extension installed for this to work:
# C/C++ Support 
# Native Debug
#

ifneq ($(strip $(wildcard $(s)/*.code-workspace)),)
$(call _tubs_log,You are in Visual Code Land! 'vscode' target available --- $(wildcard $(s)/*.code-workspacea))
_vscode_found=y
endif

_TUBS_LOADED_SUB_MAKES += vscode_support
_TUBS_ALLOWED_TOP_TARGETS += vscode vs2

__src_to_obj = $(call _o_dir,$(1))/$(call _source_to_object,$(1),$(2))

define _add_to_comp_db
{"directory": "$(patsubst %/,%,$(S))", "file": "$(patsubst $(S)%,%,$(call _s_dir,$(1))/$(2))","command": "$(patsubst _tubs_tag_%,%,$(subst \",\\\",$(file <$(call __src_to_obj,$(1),$(2)).d.flags)))"},

endef

_add_target_to_comp_db = $(if $(call _get_build_outputs,$(1)),\
                            $(foreach _s,$($(1)_sources),$(if $(wildcard $(call __src_to_obj,$(1),$(2))),$(call _add_to_comp_db,$(1),$(_s)))))

define vscode_gen_c_cpp_properties
_includePath = $(subst $(space),$(comma),$(strip $(addsuffix ",$(addprefix ",$(foreach _t,$(app_targets) $(lib_targets),$$$${workspaceFolder}/$($(_t)_dir))))))
_defines = [$(subst $(space),$(comma),$(strip $(addsuffix ",$(addprefix ",$(patsubst -D%,%,$(foreach _d,$(DEFINES),$(_d)))))))]

_settings_data =\
    { "configurations": [\
        {\
            "name": "Linux",\
            "compileCommands": "$$$${workspaceFolder}/.vscode/compile_commands.json",\
            "defines": $${_defines},\
            "compilerPath": "$(shell which $(CC))",\
            "cStandard": "gnu17",\
            "cppStandard": "gnu++17",\
            "intelliSenseMode": "linux-gcc-x64"\
        }\
    ],\
    "version": 4}

vscode: $(s).vscode
	@echo 'Settings things up for you in: $(s).vscode/c_cpp_properties.json'
	@test -e $(s).vscode || mkdir $(s).vscode
	$$(file >$(s).vscode/c_cpp_properties.json,$$(_settings_data))
	$$(file >$(s).vscode/compile_commands.json,[$$(strip $$(call for_each_target_depth,all,_add_target_to_comp_db) $$(foreach _ttt,$(_tua_tests_targets),$$(call _add_to_comp_db,$$(_ttt),$$($$(_ttt)_sources))))])
	$$(call cmd,sed,all,$(s).vscode/compile_commands.json,-e 's/}$$(comma)]/}]/g')
	$$(if $$(BOARD_IP)$$(call seq,$(TUBS_CONFIG_HW_TARGET),HOST),$$(file >$(s).vscode/launch.json,$$(call _launch_json,all)),@echo "Need to define BOARD_IP= to generate remote launcher entries")
	$$(if $$(BOARD_IP),$$(call cmd,sed,all,$(s).vscode/launch.json,-e 's/]}$$(comma)]/]}]}/g'))
	$$(file >$(s).vscode/tasks.json,$$(call _jobs_tasks,all))

endef

$(s).vscode:
	$(Q)mkdir $(s).vscode

define _jobs_task
{
    "type": "shell",\
    "label": "Rebuild: $($(1)_name)",\
    "command": "/bin/sh",\
    "args": [\
        "-c",\
        "BUILD_BOARD=$(BUILD_BOARD) Q= ./build/build $(patsubst $(S)%,%,$(call _s_dir,$(1))/$($(1)_name)/all)",\
    ],
    "presentation": {\
        "echo": true,\
        "focus": false,\
        "reveal": "never",\
        "revealProblems": "onProblem",\
        "panel": "shared",
        "showReuseMessage": false,
        "clear": false\
    },\
    "promptOnClose": true,\
    "problemMatcher": "$$gcc",\
    "group": "build"$\
},
endef
_jobs_tasks = {"tasks":[$(if $(BOARD_IP),$(foreach _t,$(app_targets),$(call _launch_task,$(_t))),)$(foreach _t,$(app_targets),$(call _jobs_task,$(_t)))],"version": "2.0.0"}


define _launch_data_cppdbg_common
    "request": "launch",\
    "cwd": "$(patsubst %/,%,$(S))",\
    "program": "$${workspaceFolder}/$(patsubst $(S)%,%,$(call _o_dir,$(1))/$($(1)_outputs))",\
    "MIMode": "gdb",\
    $(if $(BOARD_IP),"externalConsole": true$(comma))\
    "miDebuggerPath": "$(GDB)",\
    "miDebuggerArgs": " -ex 'handle all print nostop noignore'",\
    "additionalSOLibSearchPath": "$(subst $$(space),:,$(strip $(foreach _ld,$(call for_all_targets_libraries_deps,$(1)),$(call get_target_rpaths,$(_ld)))))",\
    $(if $(BOARD_IP),"stopAtConnect": true$(comma))\
    $(if $(BOARD_IP),"miDebuggerServerAddress": "$(BOARD_IP):2345"$(comma))\
    "preLaunchTask": "$(if $(BOARD_IP),$(BOARD_IP),Rebuild:) $($(1)_name)"$(comma)\
    "presentation": {\
        "echo": true,\
        "reveal": "always",\
        "focus": false,\
        "panel": "shared",\
        "showReuseMessage": true,\
        "clear": false\
    },\
    "setupCommands": [\
        $(if $(BOARD_IP), $(strip {\
            "description": "Enable pretty-printing for gdb",\
            "text": "set sysroot $(TUBS_TOOLCHAIN_PATH)/$(TUBS_TOOLCHAIN_PREFIX)/sysroot",\
            "ignoreFailures": false\
        }$(comma)))\
        {\
            "description": "Enable pretty-printing for gdb",\
            "text": "-enable-pretty-printing",\
            "ignoreFailures": true\
        }$\
    ]
endef


define _launch_data_cppdbga_attach
{
    $(call _launch_data_cppdbg_common,$(1)),
    "name": "$($(1)_name): attach to existing $(if $(BOARD_IP),gdbserver running on $(BOARD_IP):2345)",\
    "type": "cppdbg"\
},
endef

define _launch_data_cppdbg
{
    $(call _launch_data_cppdbg_common,$(1)),
    "name": "$($(1)_name): launch new instance, non-interactive",\
    "type": "cppdbg"\
},
endef

_launch_json = {"configurations": [$(foreach _t,$(app_targets),$(call _launch_data_cppdbg,$(_t))$(if $(BOARD_IP),$(call _launch_data_cppdbga_attach,$(_t),$(TRUE))))]}

define _launch_task
{
    "type": "shell",\
    "label": "$(BOARD_IP) $($(1)_name)",\
    "command": "/usr/bin/ssh",\
    "args": [\
        "root@$(BOARD_IP)",\
        "-i",\
        "$(TUBS_BOARD_SSH_KEY)",\
        "killall gdbserver; /usr/bin/gdbserver :2345 $(patsubst $(call _i_root_dir,$(1))%,%,$(call _i_dir,$(1))/$($(1)_outputs))",\
    ],
    "isBackground": true,\
    "problemMatcher": {\
        "pattern": [\
            {\
                "regexp": ".",\
                "file": 1,\
                "location": 2,\
                "message": 3\
            }\
        ],\
        "background": {\
            "activeOnStart": true,\
            "beginsPattern": ".",\
            "endsPattern": "."\
        }\
    },\
    "presentation": {\
        "echo": true,\
        "reveal": "always",\
        "focus": false,\
        "panel": "shared",\
        "showReuseMessage": true,\
        "clear": false\
    },\
    "promptOnClose": true,\
    "dependsOn": [ "Rebuild: $($(1)_name)"],\
    "group": {\
        "kind": "build",\
        "isDefault": true\
    }$\
},
endef

define vscode_gen_no_c_cppp_properties
vscode:
	@echo "vscode workspace not detected... hand-edit me I'm in $(T)/tubs.d/vscode.mk"
endef

define vscode_support_tubs_load_tail
$(if $(_vscode_found),$(eval $(vscode_gen_c_cpp_properties)),$(eval $(vscode_gen_no_c_cppp_properties)))
endef

ifeq ($(_vscode_found),y)
define vscode_support_tubs_help
	@echo " vscode             - Generate vscode settings file for intelliSense"
	@echo "                      If BOARD_IP environment variable is defined and the build targets"
	@echo "                      is a HW platform, then this will also generate launch.json with"
	@echo "                      targets to attach to a gdbserver running on the remote board or"
	@echo "                      launch new instances."

endef
else
define vscode_support_tubs_help
	@echo "   Tubs has support for generating config files for Visual Code."
	@echo "   If you are using Visual Code make sure to that a vscode workspace"
	@echo "   file exists in at root of the source tree"

endef
endif