Q ?= @
ECHO = echo
CP = /bin/cp
MV = /bin/mv
RM = /bin/rm
RMF = /bin/rm -f
MKDIR = mkdir
TOUCH = touch
PATCH = patch
SED = /bin/sed
PATCHELF = patchelf
WGET = /usr/bin/wget
ENV = /usr/bin/env
SH = /bin/sh -c
SHA1 = /usr/bin/sha1sum
SHA256 = /usr/bin/sha256sum
CAT = /bin/cat
DIFF = /usr/bin/git diff --no-index --
FAKEROOT = /usr/bin/fakeroot
SSH = /usr/bin/ssh
SCP = /usr/bin/scp
CHMOD = /bin/chmod
CHOWN = /bin/chown
CHGRP = /bin/chgrp
CMAKE = /usr/bin/cmake

JOBS ?= $(shell nproc)

TRUE  := Y
FALSE :=

# keep these in the end to keep it from messing up the syntax highlighting in peoples editors
# funky variables to make reading the makefile easier and to use special chars in an unspecial way (space and $)
__empty :=
space := $(__empty) $(__empty)
tab :=	#
q := '
dq := "
comma := ,
hash := \#
dollar := $$


