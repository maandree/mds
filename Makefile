# Copying and distribution of this file, with or without modification,
# are permitted in any medium without royalty provided the copyright
# notice and this notice are preserved.  This file is offered as-is,
# without any warranty.


# Include configurations.
include mk/config.mk


# Object files for the libary.
LIBOBJ = linked-list client-list hash-table fd-table mds-message util

# Servers and utilities.
SERVERS = mds mds-respawn mds-server mds-echo mds-registry



OBJ_mds-server_ = mds-server interception-condition client multicast  \
                  queued-interception globals signals interceptors    \
                  sending slavery reexec receiving

OBJ_mds-registry_ = mds-registry util globals reexec

OBJ_mds-server   = $(foreach O,$(OBJ_mds-server_),obj/mds-server/$(O).o)
OBJ_mds-registry = $(foreach O,$(OBJ_mds-registry_),obj/mds-registry/$(O).o)



# Build rules.

.PHONY: all
all: servers libraries

include mk/build.mk

# Set permissions on built files.

.PHONY: perms
perms: all
	sudo chown 'root:root' bin/mds
	sudo chmod 4755 bin/mds

# Clean rules.

.PHONY: clean
clean:
	-rm -rf obj bin

