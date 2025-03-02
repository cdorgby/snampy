# checks to make sure that required system dependencies are installed
# Take the contents of TUBS_SYSTEM_DEPS variable and treat each items as
# system package tha we expect to be installed. For Debian like systems
# we'll use dpkg, and eventually, for RedHat like system use RPM.

# We don't add ourselves to _TUBS_LOADED_SUB_MAKES, we don't tie
# into the build system, this file should be loaded and do it's
# thing as soon as possible.

# list of packages that are needed by the TUBS build system
TUBS_SYSTEM_DEPS += patchelf coreutils libtool-bin fakeroot pkg-config unzip u-boot-tools

ifneq ($(skip_system_checks),y)
_distro = $(strip $(shell lsb_release -i | sed -e 's/Distributor ID: *\(.*\)$$/\1/'))
ifneq ($(filter Ubuntu Debian,$(_distro)),)

$(shell dpkg --print-architecture | grep -q $(TUBS_CONFIG_ARCH))
ifneq ($(.SHELLSTATUS),0)
$(shell dpkg --print-foreign-architectures | grep -q $(TUBS_CONFIG_ARCH))
ifneq ($(.SHELLSTATUS),0)
$(info )
$(info Missing multilib support for $(TUBS_CONFIG_ARCH) run to add missing architecture:)
$(info $(space)$(space)$(space)sudo dpkg --add-architecture $(TUBS_CONFIG_ARCH))
$(info $(space)$(space)$(space)sudo apt-get update)
$(info )
$(error Missing multilib support for: $(TUBS_CONFIG_ARCH))
endif
endif

_missing = $(strip $(foreach _d,$(TUBS_SYSTEM_DEPS),\
              $(if $(shell dpkg -s $(_d) 1>/dev/null 2>/dev/null || echo not_found),$(_d))))

$(if $(_missing),\
    $(info )$(info )$(info Required packages are missing: $(_missing))$(info run to install:)$(info $(space)$(space)$(space)sudo apt -y install $(_missing))$(info )$(info )$(error missing packages))
else
$(error Running on system that I don't know what to do with: $(_distro))
endif
endif
