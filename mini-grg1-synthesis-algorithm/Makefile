# Makefile for mini-grg1-synthesis-algorithm
# GR(1) Reactive Synthesis Algorithm
#
# Build targets:
#   make          — build library and all examples
#   make test     — build and run tests
#   make examples — build examples
#   make clean    — remove build artifacts
#   make all      — build everything

CC       = gcc
CFLAGS   = -std=c11 -Wall -Wextra -Wno-unused-function -Wno-unused-parameter -O2 -g
CPPFLAGS = -Iinclude
LDFLAGS  = -lm

BUILD_DIR = build
SRC_DIR   = src
INC_DIR   = include
TEST_DIR  = tests
EX_DIR    = examples
BENCH_DIR = benches
DEMOS_DIR = demos

# Source files (C only, excluding .lean)
SRCS = $(SRC_DIR)/grg1_types.c \
       $(SRC_DIR)/grg1_spec.c \
       $(SRC_DIR)/grg1_game.c \
       $(SRC_DIR)/grg1_fixpoint.c \
       $(SRC_DIR)/grg1_synthesis.c \
       $(SRC_DIR)/grg1_bdd.c \
       $(SRC_DIR)/grg1_compose.c

OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

# Example programs
EXAMPLES = $(EX_DIR)/example_mutex_arbiter \
           $(EX_DIR)/example_fixpoint_demo \
           $(EX_DIR)/example_bdd_synthesis

# Test programs
TESTS = $(TEST_DIR)/test_grg1

# Benchmarks
BENCHES = $(BENCH_DIR)/bench_synthesis

# Default target
.PHONY: all
all: lib examples tests

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile C source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

# Build the static library
libgrg1.a: $(OBJS)
	ar rcs $@ $(OBJS)

.PHONY: lib
lib: libgrg1.a

# Build all examples
.PHONY: examples
examples: $(EXAMPLES)

$(EX_DIR)/example_mutex_arbiter: $(EX_DIR)/example_mutex_arbiter.c libgrg1.a | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) $< -L. -lgrg1 $(LDFLAGS) -o $@

$(EX_DIR)/example_fixpoint_demo: $(EX_DIR)/example_fixpoint_demo.c libgrg1.a | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) $< -L. -lgrg1 $(LDFLAGS) -o $@

$(EX_DIR)/example_bdd_synthesis: $(EX_DIR)/example_bdd_synthesis.c libgrg1.a | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) $< -L. -lgrg1 $(LDFLAGS) -o $@

# Build and run tests
.PHONY: test
test: $(TEST_DIR)/test_grg1
	./$(TEST_DIR)/test_grg1

$(TEST_DIR)/test_grg1: $(TEST_DIR)/test_grg1.c libgrg1.a | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) $< -L. -lgrg1 $(LDFLAGS) -o $@

# Build benchmarks
.PHONY: bench
bench: $(BENCH_DIR)/bench_synthesis

$(BENCH_DIR)/bench_synthesis: $(BENCH_DIR)/bench_synthesis.c libgrg1.a | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) $< -L. -lgrg1 $(LDFLAGS) -o $@

# Clean build artifacts
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
	rm -f libgrg1.a
	rm -f $(EX_DIR)/example_mutex_arbiter
	rm -f $(EX_DIR)/example_fixpoint_demo
	rm -f $(EX_DIR)/example_bdd_synthesis
	rm -f $(TEST_DIR)/test_grg1
	rm -f $(BENCH_DIR)/bench_synthesis

# Print line counts
.PHONY: count
count:
	@echo "=== Line Counts ==="
	@echo "Headers:"
	@wc -l $(INC_DIR)/*.h
	@echo ""
	@echo "Source (C):"
	@wc -l $(SRC_DIR)/*.c
	@echo ""
	@echo "Source (Lean):"
	@wc -l $(SRC_DIR)/*.lean
	@echo ""
	@echo "Tests:"
	@wc -l $(TEST_DIR)/*.c
	@echo ""
	@echo "Examples:"
	@wc -l $(EX_DIR)/*.c
	@echo ""
	@echo "--- Total C + H ---"
	@cat $(INC_DIR)/*.h $(SRC_DIR)/*.c $(TEST_DIR)/*.c $(EX_DIR)/*.c 2>/dev/null | wc -l

# Safety review (no filler patterns)
.PHONY: audit
audit:
	@echo "=== Filler Detection ==="
	@grep -rn "_fn[0-9]" $(SRC_DIR)/ $(INC_DIR)/ || echo "  _fn[N]: CLEAN"
	@grep -rn "_aux[0-9]" $(SRC_DIR)/ $(INC_DIR)/ || echo "  _aux[N]: CLEAN"
	@grep -rn "_ext[0-9][^a-z]" $(SRC_DIR)/ $(INC_DIR)/ || echo "  _ext[N]: CLEAN"
	@grep -rn "algorithm variant" $(SRC_DIR)/ || echo "  algo variant: CLEAN"
	@grep -rn "supplemental assert" $(SRC_DIR)/ || echo "  suppl assert: CLEAN"
	@echo ""
	@echo "=== Stub Detection ==="
	@grep -rn "TODO\|FIXME\|stub\|placeholder" $(SRC_DIR)/ $(INC_DIR)/ $(TEST_DIR)/ $(EX_DIR)/ || echo "  No stubs found"
	@echo ""
	@echo "=== Lean Audit ==="
	@grep -rn "\bsorry\b" $(SRC_DIR)/*.lean || echo "  No sorry in Lean"
	@grep -rn "SystemMetric\|LifecycleState\|VVActivity" $(SRC_DIR)/*.lean || echo "  No copy-paste patterns"
	@grep -rn 'traceability_matrix' $(SRC_DIR)/*.lean || echo "  No dead stubs"
	@grep -rn ":= 0\.0" $(SRC_DIR)/*.lean || echo "  No constant stubs"

.PHONY: help
help:
	@echo "GR(1) Synthesis Algorithm - Build System"
	@echo ""
	@echo "Targets:"
	@echo "  make          Build library + all examples + tests"
	@echo "  make test     Build and run tests"
	@echo "  make examples Build example programs"
	@echo "  make lib      Build static library"
	@echo "  make bench    Build benchmarks"
	@echo "  make clean    Remove build artifacts"
	@echo "  make count    Print line counts"
	@echo "  make audit    Run safety review (filler detection)"
