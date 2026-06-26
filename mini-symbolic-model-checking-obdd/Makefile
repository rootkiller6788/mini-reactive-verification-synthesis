# Makefile - mini-symbolic-model-checking-obdd
# Symbolic Model Checking with OBDDs

CC       := gcc
CFLAGS   := -std=c11 -Wall -Wextra -O2 -g
INCLUDES := -Iinclude
LDFLAGS  := -lm

SRC_DIR  := src
BLD_DIR  := build

LIB_SRCS := $(SRC_DIR)/obdd.c $(SRC_DIR)/model.c $(SRC_DIR)/ctlsmc.c $(SRC_DIR)/fixpoint.c $(SRC_DIR)/ordering.c $(SRC_DIR)/reach.c $(SRC_DIR)/params.c
LIB_OBJS := $(patsubst $(SRC_DIR)/%.c,$(BLD_DIR)/%.o,$(LIB_SRCS))

.PHONY: all clean check lib

all: lib

lib: $(LIB_OBJS)
	@echo "  AR    libobdd.a"
	@ar rcs $(BLD_DIR)/libobdd.a $(LIB_OBJS)

$(BLD_DIR)/%.o: $(SRC_DIR)/%.c | $(BLD_DIR)
	@$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BLD_DIR):
	@mkdir -p $(BLD_DIR)

clean:
	rm -rf $(BLD_DIR)

check:
	@for src in $(LIB_SRCS); do \
		$(CC) $(CFLAGS) $(INCLUDES) -c $$src -o /dev/null 2>&1 || exit 1; \
	done
	@echo "All compile checks passed."

test: lib $(BLD_DIR)/test_runner
	@$(BLD_DIR)/test_runner

$(BLD_DIR)/test_runner: tests/test_all.c $(LIB_OBJS) | $(BLD_DIR)
	@$(CC) $(CFLAGS) $(INCLUDES) tests/test_all.c $(LIB_OBJS) $(LDFLAGS) -o $@
	@echo "  LD    test_runner"
