# Copying and distribution of this file, with or without modification,
# are permitted in any medium without royalty provided the copyright
# notice and this notice are preserved.  This file is offered as-is,
# without any warranty.


# The package path prefix, if you want to install to another root, set DESTDIR to that root.
PREFIX ?= /usr
# The command path excluding prefix.
BIN ?= /bin
# The library path excluding prefix.
LIB ?= /lib
# The executable library path excluding prefix.
LIBEXEC ?= /libexec
# The header-file path excluding prefix.
INCLUDE ?= /include
# The resource path excluding prefix.
DATA ?= /share
# The command path including prefix.
BINDIR ?= $(PREFIX)$(BIN)
# The library path including prefix.
LIBDIR ?= $(PREFIX)$(LIB)
# The executable library path including prefix.
LIBEXECDIR ?= $(PREFIX)$(LIBEXEC)
# The header-file path including prefix.
INCLUDEDIR ?= $(PREFIX)$(INCLUDE)
# The resource path including prefix.
DATADIR ?= $(PREFIX)$(DATA)
# The generic documentation path including prefix.
DOCDIR ?= $(DATADIR)/doc
# The info manual documentation path including prefix.
INFODIR ?= $(DATADIR)/info
# The license base path including prefix.
LICENSEDIR ?= $(DATADIR)/licenses
# The transient directory for temporary files.
TMPDIR ?= /tmp
# The transient directory for runtime files.
RUNDIR ?= /run
# The directory for site-specific configurations.
SYSCONFDIR ?= /etc
# The directory for pseudo-devices.
DEVDIR ?= /dev
# The /proc directory
PROCDIR ?= /proc
# The /proc/self directory
SELFPROCDIR ?= $(PROCDIR)/self

# The name of the package as it should be installed.
PKGNAME ?= mds

# The user ID for the root user.
ROOT_USER_UID = 0
# The group ID for the root group.
ROOT_GROUP_GID = 0
# The group ID for the nobody group.
NOBODY_GROUP_GID = $(ROOT_GROUP_GID)
# The path of the symlink to the executed command
SELF_EXE = $(SELFPROCDIR)/exe
# The path to the directory with symlinks to each file that is open
SELF_FD = $(SELFPROCDIR)/fd
# Random number generator to use for generating a token.
TOKEN_RANDOM = $(DEVDIR)/urandom
# Pathname pattern for virtual terminals.
VT_PATH_PATTERN = $(DEVDIR)/tty%i

# The byte length of the authentication token.
TOKEN_LENGTH = 1024
# The maximum number of command line arguments to allow.
ARGC_LIMIT = 50
# The number of additional arguments a libexec server may have.
LIBEXEC_ARGC_EXTRA_LIMIT = 5
# The maximum number of display allowed on the system.
DISPLAY_MAX = 1000
# The minimum time that most have elapsed for respawning to be allowed.
RESPAWN_TIME_LIMIT_SECONDS = 5
# Pattern for the names of shared object to which states are marshalled.
SHM_PATH_PATTERN = /.proc-pid-%ji

# The name of the environment variable that indicates the index of the display.
DISPLAY_ENV = MDS_DISPLAY
# The name of the environment variable that indicates the display server's process group.
PGROUP_ENV = MDS_PGROUP
# The dot-prefixless basename of the initrc file that the master server executes.
INITRC_FILE = mdsinitrc
# The root directory of all runtime data stored by mds.
MDS_RUNTIME_ROOT_DIRECTORY = $(RUNDIR)/$(PKGNAME)
# The root directory of temporarily stored data stored by mds servers.
MDS_STORAGE_ROOT_DIRECTORY = $(TMPDIR)/.{system-directory}.$(PKGNAME)


# Optimisation level (and debug flags.)
OPTIMISE = -Og -g

# Enabled Warnings.
WARN = -Wall -Wextra -pedantic -Wdouble-promotion -Wformat=2 -Winit-self       \
       -Wmissing-include-dirs -Wtrampolines -Wfloat-equal -Wshadow             \
       -Wmissing-prototypes -Wmissing-declarations -Wredundant-decls           \
       -Wnested-externs -Winline -Wno-variadic-macros -Wsign-conversion        \
       -Wswitch-default -Wconversion -Wsync-nand -Wunsafe-loop-optimizations   \
       -Wcast-align -Wstrict-overflow -Wdeclaration-after-statement -Wundef    \
       -Wbad-function-cast -Wcast-qual -Wwrite-strings -Wlogical-op            \
       -Waggregate-return -Wstrict-prototypes -Wold-style-definition -Wpacked  \
       -Wvector-operation-performance -Wunsuffixed-float-constants             \
       -Wsuggest-attribute=const -Wsuggest-attribute=noreturn                  \
       -Wsuggest-attribute=pure -Wsuggest-attribute=format -Wnormalized=nfkc

# The C standard used in the code.
STD = gnu99

# Libraries linking flags.
ifneq ($(LIBMDSSERVER_IS_INSTALLED),y)
LDS = -pthread -Lbin -lmdsserver -lrt
else
LDS = -pthread -lmdsserver -lrt
endif

# C compiler debug flags.
DEBUG_FLAGS =
ifeq ($(DEBUG),y)
DEBUG_FLAGS += -D'DEBUG'
DEBUG_FLAGS += -D'LIBEXECDIR="$(shell pwd)/bin"'
endif

# Options for the C compiler.
C_FLAGS = $(OPTIMISE) $(WARN) -std=$(STD) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS)  \
          -ftree-vrp -fstrict-aliasing -fipa-pure-const -fstack-usage       \
          -fstrict-overflow -funsafe-loop-optimizations -fno-builtin        \
	  -D'_GNU_SOURCE' -D'PKGNAME="$(PKGNAME)"' $(DEBUG_FLAGS)


# Flags to pass into the manual compilers.
TEXIFLAGS = #--force


# Linking flags for libraries required by libmdsserver.
LIBMDSSERVER_LIBS = 

# C flags for libraries required by libmdsserver.
LIBMDSSERVER_CFLAGS = 

# Linking flags for libraries required by libmdsclient.
LIBMDSCLIENT_LIBS = -pthread

# C flags for libraries required by libmdsclient.
LIBMDSCLIENT_CFLAGS = 

