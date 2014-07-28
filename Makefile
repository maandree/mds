# Copying and distribution of this file, with or without modification,
# are permitted in any medium without royalty provided the copyright
# notice and this notice are preserved.  This file is offered as-is,
# without any warranty.


# Include configurations.
include config.mk


# Object files for the libary.
LIBOBJ = linked-list client-list hash-table fd-table mds-message util

# Servers and utilities.
SERVERS = mds mds-respawn mds-server mds-echo mds-registry



OBJ_mds-server = mds-server interception-condition client multicast  \
                 queued-interception globals signals interceptors    \
                 sending slavery reexec receiving

OBJ_mds-registry = mds-registry

OBJ_mds-server_   = $(foreach O,$(OBJ_mds-server),obj/mds-server/$(O).o)
OBJ_mds-registry_ = $(foreach O,$(OBJ_mds-registry),obj/mds-registry/$(O).o)



# Build rules.

.PHONY: all
all: servers libraries

include build.mk

# Set permissions on built files.

.PHONY: perms
perms: all
	sudo chown 'root:root' bin/mds
	sudo chmod 4755 bin/mds

# Clean rules.

.PHONY: clean
clean:
	-rm -rf obj bin

