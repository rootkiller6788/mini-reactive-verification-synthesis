CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -Iinclude -O2
LDFLAGS =

SRC_DIR = src
TEST_DIR = tests
EXAMPLE_DIR = examples
BUILD_DIR = build

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

TEST_SRCS = $(wildcard $(TEST_DIR)/*.c)
TEST_BINS = $(patsubst $(TEST_DIR)/%.c,$(BUILD_DIR)/test_%,$(TEST_SRCS))

EXAMPLE_SRCS = $(wildcard $(EXAMPLE_DIR)/*.c)
EXAMPLE_BINS = $(patsubst $(EXAMPLE_DIR)/%.c,$(BUILD_DIR)/example_%,$(EXAMPLE_SRCS))

.PHONY: all clean test examples

all: $(OBJS)
	@echo "All objects built successfully."

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/test_%: $(TEST_DIR)/%.c $(OBJS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $< $(OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/example_%: $(EXAMPLE_DIR)/%.c $(OBJS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $< $(OBJS) -o $@ $(LDFLAGS)

test: $(TEST_BINS)
	@echo "Running tests..."
	@for t in $(TEST_BINS); do echo "  $$t"; ./$$t || exit 1; done
	@echo "All tests passed."

examples: $(EXAMPLE_BINS)
	@echo "Building examples..."

clean:
	rm -rf $(BUILD_DIR)

# Convenience: build a library archive
libomega.a: $(OBJS)
	ar rcs $@ $(OBJS)
