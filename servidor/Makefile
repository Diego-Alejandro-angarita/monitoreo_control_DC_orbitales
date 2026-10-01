CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -Wpedantic -O2 -D_POSIX_C_SOURCE=200809L -pthread -Iinclude $(EXTRA)
LDFLAGS = -pthread $(EXTRA)

SRC = $(wildcard src/*.c)
OBJ = $(SRC:src/%.c=build/%.o)
BIN = servidor

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

build:
	mkdir -p build

debug:
	$(MAKE) clean
	$(MAKE) EXTRA="-g -O0 -fsanitize=address,undefined"

tsan:
	$(MAKE) clean
	$(MAKE) EXTRA="-g -O1 -fsanitize=thread"

clean:
	rm -rf build $(BIN)

-include $(OBJ:.o=.d)

.PHONY: all debug tsan clean
