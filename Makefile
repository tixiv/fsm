
FSM = fsm

SRC_DIR = compiler_c
BUILD_DIR = build
TEST_DIR = tests
AOC21_DIR = aoc_21

CFLAGS += -Wall -O0 -g -MMD -MP

C_SRCS = $(wildcard $(SRC_DIR)/*.c)

all: $(FSM)

# Create build dir
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile c -> .o
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@


O_FILES  = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(C_SRCS))
DEPS     = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.d,$(C_SRCS))

# Link
$(FSM): $(O_FILES)
	$(CC) $(CFLAGS) -o $@ $(O_FILES)


TESTS  = $(wildcard $(TEST_DIR)/*.fsm)
TESTS += $(wildcard $(AOC21_DIR)/*.fsm)

test: $(FSM)
	@for t in $(TESTS); do \
		echo "Testing $$t"; \
		./$(FSM) $$t && \
		fasm out.asm out > /dev/null && \
		./out > $$t.out && \
		diff -u $$t.expected $$t.out || exit 1; \
	done
	@echo All tests succeeded.

clean:
	rm -rf $(BUILD_DIR)/*

.PHONY: all clean

-include $(DEPS)
