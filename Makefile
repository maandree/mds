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
# The resource path excluding prefix.
DATA ?= /share
# The command path including prefix.
BINDIR ?= $(PREFIX)$(BIN)
# The library path including prefix.
LIBDIR ?= $(PREFIX)$(LIB)
# The executable library path including prefix.
LIBEXECDIR ?= $(PREFIX)$(LIBEXEC)
# The resource path including prefix.
DATADIR ?= $(PREFIX)$(DATA)
# The generic documentation path including prefix.
DOCDIR ?= $(DATADIR)/doc
# The info manual documentation path including prefix.
INFODIR ?= $(DATADIR)/info
# The license base path including prefix.
LICENSEDIR ?= $(DATADIR)/licenses

# The name of the package as it should be installed.
PKGNAME ?= mds


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
LDS = -pthread -Lbin -lmdsserver -lrt

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


# Object files for the libary
LIBOBJ = linked-list hash-table fd-table mds-message util


# Build rules.

.PHONY: all
all: obj/mds-base.o bin/mds bin/mds-respawn bin/mds-server bin/libmdsserver.so


MDS_SERVER_OBJ_ = mds-server interception-condition client multicast  \
                  queued-interception globals signals interceptors    \
                  sending slavery reexec receiving
MDS_SERVER_OBJ = obj/mds-base.o $(foreach O,$(MDS_SERVER_OBJ_),obj/mds-server/$(O).o)

bin/mds-server: $(MDS_SERVER_OBJ) bin/libmdsserver.so
	mkdir -p $(shell dirname $@)
	$(CC) $(C_FLAGS) -o $@ $(LDS) $(MDS_SERVER_OBJ)
ifeq ($(DEBUG),y)
	mv $@ $@.real
	cp test.d/mds-server $@
endif

bin/%: obj/%.o obj/mds-base.o bin/libmdsserver.so
	mkdir -p $(shell dirname $@)
	$(CC) $(C_FLAGS) -o $@ $(LDS) $< obj/mds-base.o

bin/mds: obj/mds.o bin/libmdsserver.so
	mkdir -p $(shell dirname $@)
	$(CC) $(C_FLAGS) -o $@ $(LDS) $<

obj/mds-server/%.o: src/mds-server/%.c src/mds-server/*.h src/libmdsserver/*.h
	mkdir -p $(shell dirname $@)
	$(CC) $(C_FLAGS) -Isrc -c -o $@ $<

obj/%.o: src/%.c src/*.h src/libmdsserver/*.h
	mkdir -p $(shell dirname $@)
	$(CC) $(C_FLAGS) -Isrc -c -o $@ $<


bin/libmdsserver.so: $(foreach O,$(LIBOBJ),obj/libmdsserver/$(O).o)
	mkdir -p $(shell dirname $@)
	$(CC) $(C_FLAGS) -shared -o $@ $^

obj/libmdsserver/%.o: src/libmdsserver/%.c src/libmdsserver/*.h
	mkdir -p $(shell dirname $@)
	$(CC) $(C_FLAGS) -fPIC -c -o $@ $<


# Set permissions on built files.

.PHONY: perms
perms: all
	sudo chown 'root:root' bin/mds
	sudo chmod 4755 bin/mds



# Clean rules.

.PHONY: clean
clean:
	-rm -rf obj bin

