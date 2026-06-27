
TARGET = fsm

SRC_DIR = compiler_c
BUILD_DIR = build

CFLAGS += -Wall -O2

C_SRCS = $(wildcard $(SRC_DIR)/*.c)

all: $(TARGET)

# Create build dir
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile c -> .o
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@


O_FILES  = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(C_SRCS))

# Link
$(TARGET): $(O_FILES)
	$(CC) $(CFLAGS) -o $@ $(O_FILES)

clean:
	rm -rf $(BUILD_DIR)/*

.PHONY: all clean
