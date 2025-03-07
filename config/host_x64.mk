BUILD_BOARD=host_x64
VARIANT=host

# Don't care about real uid/gid mapping when doing host only build, don't copy these into a HW or real installation build
# if using this Makefile as a reference for creating a new build target
_cur_user_id=$(strip $(shell id -u))
_cur_group_id=$(strip $(shell id -g))

TUBS_USER_TO_ID_MAP += www-data:$(_cur_user_id)
TUBS_GROUP_TO_ID_MAP += www-data:$(_cur_group_id)

# list required system packages
TUBS_SYSTEM_DEPS += libssl-dev:amd64 libpam0g-dev:amd64 libcap-ng-dev:amd64 libcurl4-openssl-dev:amd64
TUBS_SYSTEM_DEPS += libbsd-dev:amd64 libncurses-dev:amd64 libexpat1-dev:amd64 libconfig-dev:amd64
TUBS_SYSTEM_DEPS += libmnl-dev:amd64
TUBS_SYSTEM_DEPS += zlib1g-dev:amd64 libarchive-dev:amd64
TUBS_DPKG_ARCH = amd64

CPPFLAGS_DEBUG = -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable -Wno-unused-value -Wno-unused-label -Wno-unused-result -Wno-unused-local-typedefs -Wno-analyzer-use-of-uninitialized-value
CFLAGS_DEBUG = -Og -g2 -O0 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -fPIE -Wall -Wextra -pedantic -Wno-analyzer-use-of-uninitialized-value
CXXFLAGS_DEBUG = -Og -g2 -O0 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -std=c++23 -fPIE -Wall -Wextra -pedantic -Wno-analyzer-use-of-uninitialized-value
DEFINES_DEBUG = -DUNIX -D_GNU_SOURCE -DDEBUG
LDFLAGS_DEBUG = -pie
EXTRA_SYSTEM_LIBS_DEBUG = -lbsd

CPPFLAGS_RELEASE = -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable -Wno-unused-value -Wno-unused-label -Wno-unused-result -Wno-unused-local-typedefs
CFLAGS_RELEASE = -O2 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -fPIE -Wall -Wextra -pedantic
CXXFLAGS_RELEASE = -O2 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -std=c++23 -fPIE -Wall -Wextra -pedantic
DEFINES_RELEASE = -DUNIX -D_GNU_SOURCE
LDFLAGS_RELEASE = -pie 
EXTRA_SYSTEM_LIBS_RELEASE = -lbsd

# tool-chain and OS
TUBS_OS_CONFIG=amd64
TUBS_CONFIG_ARCH=amd64

TUBS_TOOLCHAIN_PATH ?= /usr
TUBS_TOOLCHAIN_PREFIX ?= amd64-pc-linux-gnu
TUBS_SKIP_TOOLCHAIN_CHECK = 1
# Complain if we can't find the toolchain
#$(if $(wildcard $(TUBS_TOOLCHAIN_PATH)/bin/$(TUBS_TOOLCHAIN_PREFIX)gcc),,$(error Toolchain with prefix '$(TUBS_TOOLCHAIN_PREFIX)' not found under '$(TUBS_TOOLCHAIN_PATH)/bin'))


AR=ar
AS=as
LD=ld
NM=nm
CC=gcc-14
GCC=gcc-14
CPP=cpp-14
CXX=g++-14
RANLIB=ranlib
READELF=readelf
STRIP=strip
OBJCOPY=objcopy
OBJDUMP=objdump
PKG_CONFIG=pkg-config

#ifeq ($(CCI_LOGGER), net)
  #LOGGER_TYPE=LOGGER_TYPE_NET
  #export LOGGER_LIB = logger-net net-udp-link-mgr
#else
  #LOGGER_TYPE=LOGGER_TYPE_SYSLOG
#endif
