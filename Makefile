
CFSM = bin/cfsm
FSM = bin/fsm
FSMD = bin/fsmd

SRC_DIR = compiler_c
BUILD_DIR = build
TEST_DIR = tests
AOC21_DIR = aoc_21

CFLAGS += -Wall -Wextra -g -O0 -MMD -MP
# -fsanitize=address -fno-omit-frame-pointer

C_SRCS = $(wildcard $(SRC_DIR)/*.c)

FSM_SRCS = $(wildcard compiler_fsm/*.fsm)
FSM_SRCS += $(wildcard stdlib/*.fsm)

FSMD_SRCS = $(FSM_SRCS)
FSMD_SRCS += $(wildcard fsmd/*.fsm)

all: $(FSM) $(FSMD)

cfsm: $(CFSM)

$(FSM): $(FSM_SRCS)
	$(FSM) compiler_fsm/fsm.fsm
	fasm out1.asm -s symbols/fsm.fas
	cp out1 $(FSM)
	symbols symbols/fsm.fas symbols/fsm.sym
	listing symbols/fsm.fas symbols/fsm.lst

$(FSMD): $(FSMD_SRCS)
	$(FSM) fsmd/fsmd.fsm
	fasm out1.asm
	cp out1 $(FSMD)

bootstrap: $(CFSM) $(FSM_SRCS)
	$(CFSM) compiler_fsm/fsm.fsm
	fasm out.asm
	./out compiler_fsm/fsm.fsm
	fasm out1.asm -s symbols/fsm.fas
	cp out1 $(FSM)
	symbols symbols/fsm.fas symbols/fsm.sym
	listing symbols/fsm.fas symbols/fsm.lst

# Create build dir
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile c -> .o
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(BUILD_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@


O_FILES  = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(C_SRCS))
O_FILES += $(BUILD_DIR)/builtin_functions.o
DEPS     = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.d,$(C_SRCS))

# Link
$(CFSM): $(O_FILES)
	$(CC) $(CFLAGS) -o $@ $(O_FILES)

$(BUILD_DIR)/builtin_functions.c: $(SRC_DIR)/builtin_functions.asm
	echo "const char *builtin_functions_asm =" >$@
	sed 's/.*/"&\\n"/' $< >> $@
	echo ";" >> $@

TESTS  = $(wildcard $(TEST_DIR)/*.fsm)
TESTS += $(wildcard $(AOC21_DIR)/*.fsm)

test: $(FSM)
	@for t in $(TESTS); do \
		echo "Testing $$t"; \
		$(FSM) $$t && \
		fasm out1.asm out > /dev/null && \
		./out > $$t.out && \
		diff -u $$t.expected $$t.out || exit 1; \
	done
	@echo All tests succeeded.

clean:
	rm -rf $(BUILD_DIR)/*

.PHONY: all clean

-include $(DEPS)
