# Copying and distribution of this file, with or without modification,
# are permitted in any medium without royalty provided the copyright
# notice and this notice are preserved.  This file is offered as-is,
# without any warranty.


# Include configurations.
include mk/config.mk


# Object files for the libary.
LIBOBJ = linked-list client-list hash-table fd-table mds-message util

# Servers and utilities.
SERVERS = mds mds-respawn mds-server mds-echo mds-registry mds-clipboard  \
          mds-kkbd mds-vt

# Utilities that do not utilise mds-base.
TOOLS = mds-kbdc

# Servers that need setuid and root owner.
SETUID_SERVERS = mds mds-kkbd mds-vt


# Object files for multi-object file binaries.
OBJ_mds-server_   = mds-server interception-condition client multicast  \
                    queued-interception globals signals interceptors    \
                    sending slavery reexec receiving

OBJ_mds-registry_ = mds-registry util globals reexec registry signals  \
                    slave

OBJ_mds-kbdc_     = mds-kbdc globals raw-data functions string tree  \
                    make-tree parse-error

OBJ_mds-server    = $(foreach O,$(OBJ_mds-server_),obj/mds-server/$(O).o)
OBJ_mds-registry  = $(foreach O,$(OBJ_mds-registry_),obj/mds-registry/$(O).o)
OBJ_mds-kbdc      = $(foreach O,$(OBJ_mds-kbdc_),obj/mds-kbdc/$(O).o)


# sed:ed .h-source file.
ifneq ($(LIBMDSSERVER_IS_INSTALLED),y)
SEDED = src/libmdsserver/config.h
else
SEDED =
endif



# Build rules.

.PHONY: all
all: doc servers libraries tools

include mk/build.mk
include mk/build-doc.mk

# Set permissions on built files.

.PHONY: perms
perms: all
	sudo chown 'root:root' $(foreach S,$(SETUID_SERVERS),bin/$(S))
	sudo chmod 4755 $(foreach S,$(SETUID_SERVERS),bin/$(S))

# Clean rules.

.PHONY: clean
clean:
	-rm -rf obj bin $(SEDED)

