# mini-computation-tree-logic-ctl Makefile
# Computation Tree Logic (CTL) Model Checker
# References: Clarke, Emerson, Sistla (1986); Baier & Katoen (2008)

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 -g -Iinclude
LDFLAGS = -lm

SRCDIR = src
INCDIR = include
TESTDIR = tests
EXAMPLEDIR = examples
BENCHDIR = benches
DEMODIR = demos
BUILDDIR = build

SRCS = $(wildcard $(SRCDIR)/*.c)
OBJS = $(patsubst $(SRCDIR)/%.c, $(BUILDDIR)/%.o, $(SRCS))
TESTS = $(wildcard $(TESTDIR)/*.c)
TESTBINS = $(patsubst $(TESTDIR)/%.c, $(BUILDDIR)/test_%, $(TESTS))
EXAMPLES = $(wildcard $(EXAMPLEDIR)/*.c)
EXAMPLEBINS = $(patsubst $(EXAMPLEDIR)/%.c, $(BUILDDIR)/example_%, $(EXAMPLES))
BENCHES = $(wildcard $(BENCHDIR)/*.c)
BENCHBINS = $(patsubst $(BENCHDIR)/%.c, $(BUILDDIR)/bench_%, $(BENCHES))

LIBCTL = $(BUILDDIR)/libctl.a

.PHONY: all clean test examples bench demo

all: $(LIBCTL)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(LIBCTL): $(OBJS)
	ar rcs $@ $^

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Test targets
test: $(LIBCTL) $(TESTBINS)
	@echo "=== Running CTL Tests ==="
	@passed=0; failed=0; \
	for t in $(TESTBINS); do \
		if [ -f $$t ]; then \
			echo "Running: $$t"; \
			if $$t; then \
				passed=$$((passed+1)); \
				echo "  PASS"; \
			else \
				failed=$$((failed+1)); \
				echo "  FAIL"; \
			fi; \
		fi; \
	done; \
	echo "=== Results: $$passed passed, $$failed failed ==="; \
	[ $$failed -eq 0 ] || exit 1

$(BUILDDIR)/test_%: $(TESTDIR)/%.c $(LIBCTL)
	$(CC) $(CFLAGS) $< -L$(BUILDDIR) -lctl $(LDFLAGS) -o $@

# Example targets
examples: $(EXAMPLEBINS)

$(BUILDDIR)/example_%: $(EXAMPLEDIR)/%.c $(LIBCTL)
	$(CC) $(CFLAGS) $< -L$(BUILDDIR) -lctl $(LDFLAGS) -o $@

# Bench targets
bench: $(BENCHBINS)
	@for b in $(BENCHBINS); do if [ -f $$b ]; then $$b; fi; done

$(BUILDDIR)/bench_%: $(BENCHDIR)/%.c $(LIBCTL)
	$(CC) $(CFLAGS) $< -L$(BUILDDIR) -lctl $(LDFLAGS) -o $@

# Demo target
demo: $(BUILDDIR)/demo_ctl
	$(BUILDDIR)/demo_ctl

$(BUILDDIR)/demo_ctl: $(DEMODIR)/demo_ctl.c $(LIBCTL)
	$(CC) $(CFLAGS) $< -L$(BUILDDIR) -lctl $(LDFLAGS) -o $@

# Line count
count:
	@echo "include/ lines:"
	@cat $(INCDIR)/*.h 2>/dev/null | wc -l
	@echo "src/ lines:"
	@cat $(SRCDIR)/*.c 2>/dev/null | wc -l
	@echo "Total include/+src/:"
	@cat $(INCDIR)/*.h $(SRCDIR)/*.c 2>/dev/null | wc -l

clean:
	rm -rf $(BUILDDIR)
