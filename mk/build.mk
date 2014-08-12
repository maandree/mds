# Copying and distribution of this file, with or without modification,
# are permitted in any medium without royalty provided the copyright
# notice and this notice are preserved.  This file is offered as-is,
# without any warranty.


.PHONY: libraries
libraries: bin/libmdsserver.so

.PHONY: servers
servers: $(foreach S,$(SERVERS),bin/$(S))


# Link large servers.

ifneq ($(LIBMDSSERVER_IS_INSTALLED),y)
bin/mds-server: $(OBJ_mds-server) obj/mds-base.o src/mds-server/*.h bin/libmdsserver.so
else
bin/mds-server: $(OBJ_mds-server) obj/mds-base.o src/mds-server/*.h
endif
	mkdir -p $(shell dirname $@)
	$(CC) $(C_FLAGS) -o $@ $(LDS) $(LDS_mds-server) $(OBJ_mds-server) obj/mds-base.o
ifeq ($(DEBUG),y)
	mv $@ $@.real
	cp test.d/mds-server $@
endif


ifneq ($(LIBMDSSERVER_IS_INSTALLED),y)
bin/mds-registry: $(OBJ_mds-registry) obj/mds-base.o src/mds-registry/*.h bin/libmdsserver.so
else
bin/mds-registry: $(OBJ_mds-registry) obj/mds-base.o src/mds-registry/*.h
endif
	mkdir -p $(shell dirname $@)
	$(CC) $(C_FLAGS) -o $@ $(LDS) $(LDS_mds-registry) $(OBJ_mds-registry) obj/mds-base.o
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
	mkdir -p $(shell dirname $@)
	$(CC) $(C_FLAGS) -o $@ $(LDS) $(LDS_mds) $<


# Build object files for kernel/servers.

ifneq ($(LIBMDSSERVER_IS_INSTALLED),y)
obj/%.o: src/%.c $(shell dirname src/%)/*.h src/mds-base.h src/libmdsserver/*.h
	mkdir -p $(shell dirname $@)
	$(CC) $(C_FLAGS) -Isrc -c -o $@ $<

else
obj/%.o: src/%.c $(shell dirname src/%)/*.h src/mds-base.h
	mkdir -p $(shell dirname $@)
	$(CC) $(C_FLAGS) -c -o $@ $<
endif


# Build libmdsserver.

bin/libmdsserver.so: $(foreach O,$(LIBOBJ),obj/libmdsserver/$(O).o)
	mkdir -p $(shell dirname $@)
	$(CC) $(C_FLAGS) -shared -o $@ $^

obj/libmdsserver/%.o: src/libmdsserver/%.c src/libmdsserver/*.h
	mkdir -p $(shell dirname $@)
	$(CC) $(C_FLAGS) -fPIC -c -o $@ $<

