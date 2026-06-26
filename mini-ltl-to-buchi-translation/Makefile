CC       = gcc
CFLAGS   = -Wall -Wextra -std=c11 -O2 -g
AR       = ar
ARFLAGS  = rcs

INCDIR   = include
SRCDIR   = src
TESTDIR  = tests
EXAMDIR  = examples
BUILDDIR = build

SRCS = $(SRCDIR)/ltl.c $(SRCDIR)/buchi.c $(SRCDIR)/ltl_to_buchi.c $(SRCDIR)/ltl_property.c

OBJS = $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SRCS))

LIB = $(BUILDDIR)/libltlbuchi.a

HEADERS = $(INCDIR)/ltl.h $(INCDIR)/buchi.h $(INCDIR)/ltl_to_buchi.h

TEST_SRCS = $(TESTDIR)/test_all.c
TEST_BIN  = $(BUILDDIR)/test_runner

.PHONY: all test examples clean

all: $(LIB)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c $(HEADERS) | $(BUILDDIR)
	$(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

$(LIB): $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

$(BUILDDIR)/test_all.o: $(TESTDIR)/test_all.c $(HEADERS) | $(BUILDDIR)
	$(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

$(TEST_BIN): $(BUILDDIR)/test_all.o $(LIB)
	$(CC) $(CFLAGS) -I$(INCDIR) $< -L$(BUILDDIR) -lltlbuchi -o $@ -mconsole

test: $(TEST_BIN)
	@echo '=== Running Tests ==='
	@$(TEST_BIN)
	@echo '=== All Tests Passed ==='

clean:
	rm -rf $(BUILDDIR)
