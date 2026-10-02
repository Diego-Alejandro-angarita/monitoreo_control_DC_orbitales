CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -Wpedantic -O2 -D_POSIX_C_SOURCE=200809L -pthread -Iinclude $(EXTRA)
LDFLAGS = -pthread $(EXTRA)

SRC = $(wildcard src/*.c)
OBJ = $(SRC:src/%.c=build/%.o)
BIN = servidor
TEST_OBJ = $(filter-out build/main.o, $(OBJ))

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

test: build/test_protocolo
	./build/test_protocolo

build/test_protocolo: tests/test_protocolo.c $(TEST_OBJ) | build
	$(CC) $(CFLAGS) $< $(TEST_OBJ) -o $@ $(LDFLAGS)

tsan:
	$(MAKE) clean
	$(MAKE) EXTRA="-g -O1 -fsanitize=thread"

clean:
	rm -rf build $(BIN)

-include $(OBJ:.o=.d)

.PHONY: all debug test tsan clean
