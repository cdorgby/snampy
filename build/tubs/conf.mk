
include $(S)config/$(TARGET).mk

ifeq ($(VARIANT),)
$(error VARIANT not defined in $(S)config/$(TARGET).mk)
endif

# TUBS_SKIP_TOOLCHAIN_CHECK is used to skip setting up the toolchain paths. This is useful when the toolchain is already
# setup in the environment. (e.g. when using host's toolchain)
ifneq "$(TUBS_SKIP_TOOLCHAIN_CHECK)" "1"
    ifeq "$(wildcard $(TUBS_TOOLCHAIN_PATH)/bin/$(TUBS_TOOLCHAIN_PREFIX)-gcc)" ""
        # Download our own
        TUBS_TOOLCHAIN_PATH := $(s)toolchains/$(TUBS_TOOLCHAIN_DIR)
        TUBS_TOOLCHAIN_DEP = $(CC) $(CXX) $(LD)
    endif
    AR=$(TUBS_TOOLCHAIN_PATH)/bin/$(TUBS_TOOLCHAIN_PREFIX)-ar
    AS=$(TUBS_TOOLCHAIN_PATH)/bin/$(TUBS_TOOLCHAIN_PREFIX)-as
    LD=$(TUBS_TOOLCHAIN_PATH)/bin/$(TUBS_TOOLCHAIN_PREFIX)-ld
    NM=$(TUBS_TOOLCHAIN_PATH)/bin/$(TUBS_TOOLCHAIN_PREFIX)-nm
    CC=$(TUBS_TOOLCHAIN_PATH)/bin/$(TUBS_TOOLCHAIN_PREFIX)-gcc
    GCC=$(TUBS_TOOLCHAIN_PATH)/bin/$(TUBS_TOOLCHAIN_PREFIX)-gcc
    CPP=$(TUBS_TOOLCHAIN_PATH)/bin/$(TUBS_TOOLCHAIN_PREFIX)-cpp
    CXX=$(TUBS_TOOLCHAIN_PATH)/bin/$(TUBS_TOOLCHAIN_PREFIX)-g++
    GDB=$(TUBS_TOOLCHAIN_PATH)/bin/$(TUBS_TOOLCHAIN_PREFIX)-gdb
    RANLIB=$(TUBS_TOOLCHAIN_PATH)/bin/$(TUBS_TOOLCHAIN_PREFIX)-ranlib
    READELF=$(TUBS_TOOLCHAIN_PATH)/bin/$(TUBS_TOOLCHAIN_PREFIX)-readelf
    STRIP=$(TUBS_TOOLCHAIN_PATH)/bin/$(TUBS_TOOLCHAIN_PREFIX)-strip
    OBJCOPY=$(TUBS_TOOLCHAIN_PATH)/bin/$(TUBS_TOOLCHAIN_PREFIX)-objcopy
    OBJDUMP=$(TUBS_TOOLCHAIN_PATH)/bin/$(TUBS_TOOLCHAIN_PREFIX)-objdump
    PKG_CONFIG=$(TUBS_TOOLCHAIN_PATH)/bin/pkg-config
endif


TUBS_ARCHIVE_DIR ?= 
TUBS_TOOLCHAINS_DIR ?= $(s)toolchains

TUBS_CONFIG_BUILD_TYPE ?= debug

ifeq ($(TUBS_CONFIG_BUILD_TYPE),debug)
TUBS_CONFIG_DEBUG = $(TRUE)
TUBS_CONFIG_RELEASE = $(FALSE)

CPPFLAGS = $(CPPFLAGS_DEBUG)
CFLAGS = $(CFLAGS_DEBUG)
CXXFLAGS = $(CXXFLAGS_DEBUG)
LDFLAGS = $(LDFLAGS_DEBUG)

TUBS_CPPFLAGS = $(DEFINES_DEBUG)
TUBS_EXTRA_SYSTEM_LIBS = $(EXTRA_SYSTEM_LIBS_DEBUG)
else ifeq ($(TUBS_CONFIG_BUILD_TYPE),release)
TUBS_CONFIG_DEBUG = $(FALSE)
TUBS_CONFIG_RELEASE = $(TRUE)

CPPFLAGS = $(CPPFLAGS_RELEASE)
CFLAGS = $(CFLAGS_RELEASE)
CXXFLAGS = $(CXXFLAGS_RELEASE)
LDFLAGS = $(LDFLAGS_RELEASE)

TUBS_CPPFLAGS = $(DEFINES_RELEASE)
TUBS_EXTRA_SYSTEM_LIBS = $(EXTRA_SYSTEM_LIBS_RELEASE)
endif

# This is the name of the build target.
TUBS_CONFIG_HW_TARGET ?= HOST

# List of default user/group id mappings, should be extended in the BUILB_BOARD config
# file. They should be in the form of user:id
TUBS_USER_TO_ID_MAP += root:0
TUBS_GROUP_TO_ID_MAP += root:0

# this is the default architecture we need dpkg to support
# configs like host_i386 should override this for their own needs
TUBS_DPKG_ARCH ?= amd64

# Default SSH key used to access the target board during development
TUBS_BOARD_SSH_KEY ?=

# location of kernel build directory
TUBS_KERNEL_BUILD_DIR ?=

# additional path to pass to the PKG_CONFIG_PATH when building external projects
TUBS_PKG_CONFIG_PATH ?=