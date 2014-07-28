# Copying and distribution of this file, with or without modification,
# are permitted in any medium without royalty provided the copyright
# notice and this notice are preserved.  This file is offered as-is,
# without any warranty.


.PHONY: libraries
libraries: bin/libmdsserver.so

.PHONY: servers
servers: $(foreach S,$(SERVERS),bin/$(S))


# Link large servers.

bin/mds-server: $(OBJ_mds-server_) obj/mds-base.o bin/libmdsserver.so
	mkdir -p $(shell dirname $@)
	$(CC) $(C_FLAGS) -o $@ $(LDS) $(LDS_mds-server) $(OBJ_mds-server_) obj/mds-base.o
ifeq ($(DEBUG),y)
	mv $@ $@.real
	cp test.d/mds-server $@
endif


# Link small servers.

bin/%: obj/%.o obj/mds-base.o bin/libmdsserver.so
	mkdir -p $(shell dirname $@)
	$(CC) $(C_FLAGS) -o $@ $(LDS) $(LDS_$*) $< obj/mds-base.o


# Link kernel.

bin/mds: obj/mds.o bin/libmdsserver.so
	mkdir -p $(shell dirname $@)
	$(CC) $(C_FLAGS) -o $@ $(LDS) $(LDS_mds) $<


# Build object files for kernel/servers.

obj/%.o: src/%.c $(shell dirname src/%)/*.h src/mds-base.h src/libmdsserver/*.h
	mkdir -p $(shell dirname $@)
	$(CC) $(C_FLAGS) -Isrc -c -o $@ $<


# Build libmdsserver.

bin/libmdsserver.so: $(foreach O,$(LIBOBJ),obj/libmdsserver/$(O).o)
	mkdir -p $(shell dirname $@)
	$(CC) $(C_FLAGS) -shared -o $@ $^

obj/libmdsserver/%.o: src/libmdsserver/%.c src/libmdsserver/*.h
	mkdir -p $(shell dirname $@)
	$(CC) $(C_FLAGS) -fPIC -c -o $@ $<

