CC = gcc
CFLAGS = -Wall -Wextra -O2 -I./include
AR = ar

SRC = $(wildcard src/*.c)
OBJ = $(patsubst src/%.c,build/%.o,$(SRC))
LIB = build/libltl.a

EXAMPLES_SRC = $(wildcard examples/*.c)
EXAMPLES_BIN = $(patsubst examples/%.c,build/example_%,$(EXAMPLES_SRC))

TESTS_SRC = $(wildcard tests/*.c)
TESTS_BIN = $(patsubst tests/%.c,build/test_%,$(TESTS_SRC))

all: $(LIB) tests examples

$(LIB): $(OBJ)
	@mkdir -p build
	$(AR) rcs $@ $^

build/%.o: src/%.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/test_%: tests/%.c $(LIB)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ $< -Lbuild -lltl -lm

test: $(TESTS_BIN)
	@for bin in $(TESTS_BIN); do echo "=== $$bin ===" && ./$$bin && echo ""; done

build/example_%: examples/%.c $(LIB)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ $< -Lbuild -lltl -lm

examples: $(EXAMPLES_BIN)

run-examples: examples
	@for bin in $(EXAMPLES_BIN); do echo "=== $$bin ===" && ./$$bin && echo ""; done

clean:
	rm -rf build/

.PHONY: all test clean examples run-examples
