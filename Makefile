
# this can be the release version
UPDATE_BUNDLE_VERSION=$(shell $(T)/scripts/git_utils.sh get_build_version)
GIT_DESCRIBE := $(shell git describe --long --dirty)
PROJECT_NAME = snampy

# This is needed since there is no standard way to extract
# the kernel version from different kernel images
#UPDATE_KERNEL_VERSION=$(shell cd OS && $(T)/scripts/git_utils.sh get_build_version)

subdirs = libs apps

pkgsfx = apps configs

# leave apps without install root, it will be our default
#webapp_install_root = webapp

#configs_install_root = config
#configs_unpack_prefix = /mnt/config

# setup to copy kernel from buildroot's to our output directory
# ifneq ($(TUBS_CONFIG_ARCH),i386)
#ifneq "$(TUBS_KERNEL_IMAGE)" ""
#externals = OS
#endif

#OS_outputs = $(TUBS_KERNEL_IMAGE)

# install kernel into /boot
#OS_install_root = boot
#OS_post_makefile = Make-OS.mk
#OS_ext_build = none
# tell the build that we don't have a source dir to setup
#OS_source_skip = $(TRUE)
