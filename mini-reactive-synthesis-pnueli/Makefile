# Makefile for mini-reactive-synthesis-pnueli
# Pnueli-style Reactive Synthesis

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 -g
INCLUDES = -I./include
LDFLAGS = -lm

SRCDIR = src
INCDIR = include
TSTDIR = tests
EXMDIR = examples
BLDDIR = build

SRCS = $(SRCDIR)/reactive_core.c $(SRCDIR)/ltl_parser.c $(SRCDIR)/game_solver.c $(SRCDIR)/gr1_synthesis.c $(SRCDIR)/synthesis_engine.c $(SRCDIR)/automaton_ops.c $(SRCDIR)/bounded_synthesis.c

OBJS = $(SRCS:$(SRCDIR)/%.c=$(BLDDIR)/%.o)

.PHONY: all clean test examples lib count

all: lib

lib: $(OBJS)
	@echo "Library objects built successfully"

$(BLDDIR)/%.o: $(SRCDIR)/%.c | $(BLDDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BLDDIR):
	mkdir -p $(BLDDIR)

test: $(BLDDIR)/test_runner
	./$(BLDDIR)/test_runner

$(BLDDIR)/test_runner: $(TSTDIR)/test_all.c $(OBJS) | $(BLDDIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(OBJS) -o $@ $(LDFLAGS)

clean:
	rm -rf $(BLDDIR)

count:
	@echo "include/ lines:" && find $(INCDIR) -name "*.h" -exec cat {} + 2>/dev/null | wc -l
	@echo "src/ lines (.c):" && find $(SRCDIR) -name "*.c" -exec cat {} + 2>/dev/null | wc -l
	@echo "src/ lines (.lean):" && find $(SRCDIR) -name "*.lean" -exec cat {} + 2>/dev/null | wc -l
