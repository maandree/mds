# Copying and distribution of this file, with or without modification,
# are permitted in any medium without royalty provided the copyright
# notice and this notice are preserved.  This file is offered as-is,
# without any warranty.


.PHONY: libraries
libraries: libmdsserver libmdsclient

.PHONY: libmdsserver
libmdsserver: $(foreach EXT,so so.$(LIBMDSSERVER_MAJOR) so.$(LIBMDSSERVER_VERSION) pc,bin/libmdsserver.$(EXT))

.PHONY: libmdsclient
libmdsclient: $(foreach EXT,so so.$(LIBMDSCLIENT_MAJOR) so.$(LIBMDSCLIENT_VERSION) pc,bin/libmdsclient.$(EXT))

.PHONY: servers
servers: $(foreach S,$(SERVERS),bin/$(S))

.PHONY: tools
tools: $(foreach T,$(TOOLS),bin/$(T))


# Link large servers.

ifneq ($(LIBMDSSERVER_IS_INSTALLED),y)
bin/mds-server: $(OBJ_mds-server) obj/mds-base.o bin/libmdsserver.so
else
bin/mds-server: $(OBJ_mds-server) obj/mds-base.o
endif
	mkdir -p $(shell dirname $@)
	$(CC) $(C_FLAGS) -o $@ $(LDS) $(LDS_mds-server) $(OBJ_mds-server) obj/mds-base.o
ifeq ($(DEBUG),y)
	mv $@ $@.real
	cp test.d/mds-server $@
endif


ifneq ($(LIBMDSSERVER_IS_INSTALLED),y)
bin/mds-registry: $(OBJ_mds-registry) obj/mds-base.o bin/libmdsserver.so
else
bin/mds-registry: $(OBJ_mds-registry) obj/mds-base.o
endif
	mkdir -p $(shell dirname $@)
	$(CC) $(C_FLAGS) -o $@ $(LDS) $(LDS_mds-registry) $(OBJ_mds-registry) obj/mds-base.o


# Link small servers.

ifneq ($(LIBMDSSERVER_IS_INSTALLED),y)
bin/%: obj/%.o obj/mds-base.o bin/libmdsserver.so
else
bin/%: obj/%.o obj/mds-base.o
endif
	mkdir -p $(shell dirname $@)
	$(CC) $(C_FLAGS) -o $@ $(LDS) $(LDS_$*) $< obj/mds-base.o


# Link kernel.

ifneq ($(LIBMDSSERVER_IS_INSTALLED),y)
bin/mds: obj/mds.o bin/libmdsserver.so
else
bin/mds: obj/mds.o
endif
	mkdir -p $(shell dirname $@)
	$(CC) $(C_FLAGS) -o $@ $(LDS) $(LDS_mds) $<


# Link utilies that do not use mds-base.

ifneq ($(LIBMDSSERVER_IS_INSTALLED),y)
bin/mds-kbdc: $(OBJ_mds-kbdc) bin/libmdsserver.so
else
bin/mds-kbdc: $(OBJ_mds-kbdc)
endif
	mkdir -p $(shell dirname $@)
	$(CC) $(C_FLAGS) -o $@ $(LDS) $(LDS_mds-kbdc) $(OBJ_mds-kbdc)


# Build object files for kernel/servers/utilities.

ifneq ($(LIBMDSSERVER_IS_INSTALLED),y)
obj/%.o: src/%.c src/%.h src/mds-base.h src/libmdsserver/*.h $(SEDED)
	mkdir -p $(shell dirname $@)
	$(CC) $(C_FLAGS) -Isrc -c -o $@ $<
obj/%.o: src/%.c src/mds-base.h src/libmdsserver/*.h $(SEDED)
	mkdir -p $(shell dirname $@)
	$(CC) $(C_FLAGS) -Isrc -c -o $@ $<
obj/mds-server/%.o: src/mds-server/%.c src/mds-server/*.h src/mds-base.h src/libmdsserver/*.h $(SEDED)
	mkdir -p $(shell dirname $@)
	$(CC) $(C_FLAGS) -Isrc -c -o $@ $<
obj/mds-registry/%.o: src/mds-registry/%.c src/mds-registry/*.h src/mds-base.h src/libmdsserver/*.h $(SEDED)
	mkdir -p $(shell dirname $@)
	$(CC) $(C_FLAGS) -Isrc -c -o $@ $<
obj/mds-kbdc/%.o: src/mds-kbdc/%.c src/mds-kbdc/*.h src/libmdsserver/*.h $(SEDED)
	mkdir -p $(shell dirname $@)
	$(CC) $(C_FLAGS) -Isrc -c -o $@ $<
else
obj/%.o: src/%.c src/%.h src/mds-base.h $(SEDED)
	mkdir -p $(shell dirname $@)
	$(CC) $(C_FLAGS) -c -o $@ $<
obj/%.o: src/%.c src/mds-base.h $(SEDED)
	mkdir -p $(shell dirname $@)
	$(CC) $(C_FLAGS) -c -o $@ $<
obj/mds-server/%.o: src/mds-server/%.c src/mds-server/*.h src/mds-base.h $(SEDED)
	mkdir -p $(shell dirname $@)
	$(CC) $(C_FLAGS) -c -o $@ $<
obj/mds-registry/%.o: src/mds-registry/%.c src/mds-registry/*.h src/mds-base.h $(SEDED)
	mkdir -p $(shell dirname $@)
	$(CC) $(C_FLAGS) -c -o $@ $<
obj/mds-kbdc/%.o: src/mds-kbdc/%.c src/mds-kbdc/*.h $(SEDED)
	mkdir -p $(shell dirname $@)
	$(CC) $(C_FLAGS) -c -o $@ $<
endif


# Build libmdsserver.

bin/libmdsserver.so.$(LIBMDSSERVER_VERSION): $(foreach O,$(SERVEROBJ),obj/libmdsserver/$(O).o)
	mkdir -p $(shell dirname $@)
	$(CC) $(C_FLAGS) -shared -Wl,-soname,libmdsclient.so.$(LIBMDSSERVER_MAJOR) -o $@ $^

bin/libmdsserver.so.$(LIBMDSSERVER_MAJOR): bin/libmdsserver.so.$(LIBMDSSERVER_VERSION)
	ln -sf libmdsserver.so.$(LIBMDSSERVER_VERSION) $@

bin/libmdsserver.so: bin/libmdsserver.so.$(LIBMDSSERVER_VERSION)
	ln -sf libmdsserver.so.$(LIBMDSSERVER_VERSION) $@

obj/libmdsserver/%.o: src/libmdsserver/%.c src/libmdsserver/*.h $(SEDED)
	mkdir -p $(shell dirname $@)
	$(CC) $(C_FLAGS) -fPIC -c -o $@ $<

# sed header files.
ifneq ($(LIBMDSSERVER_IS_INSTALLED),y)
src/libmdsserver/config.h: src/libmdsserver/config.h.in
	cp $< $@
	sed -i 's:@PKGNAME@:$(PKGNAME):g' $@
	sed -i 's:@LIBEXECDIR@:$(LIBEXECDIR):g' $@
	sed -i 's:@TMPDIR@:$(TMPDIR):g' $@
	sed -i 's:@RUNDIR@:$(RUNDIR):g' $@
	sed -i 's:@SYSCONFDIR@:$(SYSCONFDIR):g' $@
	sed -i 's:@DEVDIR@:$(DEVDIR):g' $@
	sed -i 's:@ROOT_USER_UID@:$(ROOT_USER_UID):g' $@
	sed -i 's:@ROOT_GROUP_GID@:$(ROOT_GROUP_GID):g' $@
	sed -i 's:@NOBODY_GROUP_GID@:$(NOBODY_GROUP_GID):g' $@
	sed -i 's:@TOKEN_LENGTH@:$(TOKEN_LENGTH):g' $@
	sed -i 's:@ARGC_LIMIT@:$(ARGC_LIMIT):g' $@
	sed -i 's:@LIBEXEC_ARGC_EXTRA_LIMIT@:$(LIBEXEC_ARGC_EXTRA_LIMIT):g' $@
	sed -i 's:@DISPLAY_MAX@:$(DISPLAY_MAX):g' $@
	sed -i 's:@RESPAWN_TIME_LIMIT_SECONDS@:$(RESPAWN_TIME_LIMIT_SECONDS):g' $@
	sed -i 's:@DISPLAY_ENV@:$(DISPLAY_ENV):g' $@
	sed -i 's:@PGROUP_ENV@:$(PGROUP_ENV):g' $@
	sed -i 's:@INITRC_FILE@:$(INITRC_FILE):g' $@
	sed -i 's:@SELF_EXE@:$(SELF_EXE):g' $@
	sed -i 's:@SELF_FD@:$(SELF_FD):g' $@
	sed -i 's:@TOKEN_RANDOM@:$(TOKEN_RANDOM):g' $@
	sed -i 's:@VT_PATH_PATTERN@:$(VT_PATH_PATTERN):g' $@
	sed -i 's:@SHM_PATH_PATTERN@:$(SHM_PATH_PATTERN):g' $@
	sed -i 's:@MDS_RUNTIME_ROOT_DIRECTORY@:$(MDS_RUNTIME_ROOT_DIRECTORY):g' $@
	sed -i 's:@MDS_STORAGE_ROOT_DIRECTORY@:$(MDS_STORAGE_ROOT_DIRECTORY):g' $@
endif

bin/libmdsserver.pc: src/libmdsserver/libmdsserver.pc.in
	mkdir -p $(shell dirname $@)
	cp $< $@
	sed -i 's:@LIBDIR@:$(LIBDIR):g' $@
	sed -i 's:@INCLUDEDIR@:$(INCLUDEDIR):g' $@
	sed -i 's:@VERSION@:$(LIBMDSSERVER_VERSION):g' $@
	sed -i 's:@LIBS@:$(LIBMDSSERVER_LIBS):g' $@
	sed -i 's:@CFLAGS@:$(LIBMDSSERVER_CFLAGS):g' $@


# Build libmdsclient.

bin/libmdsclient.so.$(LIBMDSCLIENT_VERSION): $(foreach O,$(CLIENTOBJ),obj/libmdsclient/$(O).o)
	mkdir -p $(shell dirname $@)
	$(CC) $(C_FLAGS) -shared -Wl,-soname,libmdsclient.so.$(LIBMDSCLIENT_MAJOR) -o $@ $^

bin/libmdsclient.so.$(LIBMDSCLIENT_MAJOR): bin/libmdsclient.so.$(LIBMDSCLIENT_VERSION)
	ln -sf libmdsclient.so.$(LIBMDSCLIENT_VERSION) $@

bin/libmdsclient.so: bin/libmdsclient.so.$(LIBMDSCLIENT_VERSION)
	ln -sf libmdsclient.so.$(LIBMDSCLIENT_VERSION) $@

obj/libmdsclient/%.o: src/libmdsclient/%.c src/libmdsclient/*.h $(SEDED)
	mkdir -p $(shell dirname $@)
	$(CC) $(C_FLAGS) -fPIC -c -o $@ $<

bin/libmdsclient.pc: src/libmdsclient/libmdsclient.pc.in
	mkdir -p $(shell dirname $@)
	cp $< $@
	sed -i 's:@LIBDIR@:$(LIBDIR):g' $@
	sed -i 's:@INCLUDEDIR@:$(INCLUDEDIR):g' $@
	sed -i 's:@VERSION@:$(LIBMDSCLIENT_VERSION):g' $@
	sed -i 's:@LIBS@:$(LIBMDSCLIENT_LIBS):g' $@
	sed -i 's:@CFLAGS@:$(LIBMDSCLIENT_CFLAGS):g' $@

