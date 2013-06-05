CFLAGS := $(CFLAGS) -Wall -O2 -mtune=native -g
MFLAGS := -shared -fPIC
INC    := -Iinclude $(INC)
LFLAGS := -levent -ldl -Wl,--export-dynamic
CC     := gcc
BINARY := mtop
MODULES:= modules/sample.so
DEPS   := build/main.o build/debug.o build/config.o build/listener.o build/module.o build/client.o \
build/websocket.o build/sha1.o build/base64.o

.PHONY: all clean

all: build $(DEPS) bin/$(BINARY) $(MODULES)

build:
	-mkdir -p build bin

build/main.o: src/main.c
	$(CC) $(CFLAGS) $(INC) -c -o build/main.o src/main.c

build/debug.o: src/debug.c include/debug.h
	$(CC) $(CFLAGS) $(INC) -c -o build/debug.o src/debug.c

build/config.o: src/config.c include/config.h
	$(CC) $(CFLAGS) $(INC) -c -o build/config.o src/config.c

build/listener.o: src/listener.c include/listener.h
	$(CC) $(CFLAGS) $(INC) -c -o build/listener.o src/listener.c

build/module.o: src/module.c include/module.h
	$(CC) $(CFLAGS) $(INC) -c -o build/module.o src/module.c

build/client.o: src/client.c include/client.h
	$(CC) $(CFLAGS) $(INC) -c -o build/client.o src/client.c

build/websocket.o: src/websocket.c include/websocket.h
	$(CC) $(CFLAGS) $(INC) -c -o build/websocket.o src/websocket.c

build/sha1.o: src/sha1.c include/sha1.h
	$(CC) $(CFLAGS) $(INC) -c -o build/sha1.o src/sha1.c

build/base64.o: src/base64.c include/base64.h
	$(CC) $(CFLAGS) $(INC) -c -o build/base64.o src/base64.c

modules/sample.so: modules/src/sample.c
	$(CC) $(CFLAGS) $(MFLAGS) $(DEFINES) $(INC) -o modules/sample.so modules/src/sample.c

bin/$(BINARY): $(DEPS)
	$(CC) $(CFLAGS) $(INC) -o bin/$(BINARY) $(DEPS) $(LFLAGS)

clean:
	rm -rfv build bin

clang:
	$(MAKE) CC=clang
