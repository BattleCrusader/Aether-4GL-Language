HOST_CC = gcc
HOST_CFLAGS = -std=c11 -Wall -Wextra -g -O0 -Iinclude -D_GNU_SOURCE \
	-DLD='"x86_64-elf-ld"'

BUILD_DIR = build
SRC_DIR = src
TEST_DIR = tests

CORE_SRCS = \
	src/arena.c \
	src/vector.c \
	src/str.c \
	src/tokenizer.c \
	src/lexer.c \
	src/ast.c \
	src/parser.c \
	src/semantic.c \
	src/codegen.c

CORE_OBJS = $(CORE_SRCS:src/%.c=$(BUILD_DIR)/%.o)

.PHONY: all clean test tokenizer parser-test aether-cli

all: tokenizer parser-test aether-cli

# Pattern: compile src/*.c to build/*.o
$(BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(@D)
	$(HOST_CC) $(HOST_CFLAGS) -c $< -o $@

# Pattern: compile tests/*.c to build/*.o
$(BUILD_DIR)/%.o: tests/%.c
	@mkdir -p $(@D)
	$(HOST_CC) $(HOST_CFLAGS) -c $< -o $@

# Link test executables
$(BUILD_DIR)/test_tokenizer: $(CORE_OBJS) $(BUILD_DIR)/test_tokenizer.o
	$(HOST_CC) $(HOST_CFLAGS) -o $@ $^

$(BUILD_DIR)/test_parser: $(CORE_OBJS) $(BUILD_DIR)/test_parser.o
	$(HOST_CC) $(HOST_CFLAGS) -o $@ $^

$(BUILD_DIR)/aether.o: src/aether.c
	@mkdir -p $(@D)
	$(HOST_CC) $(HOST_CFLAGS) -c $< -o $@

$(BUILD_DIR)/aether: $(CORE_OBJS) $(BUILD_DIR)/aether.o
	$(HOST_CC) $(HOST_CFLAGS) -o $@ $^

# Convenience targets
tokenizer: $(BUILD_DIR)/test_tokenizer
parser-test: $(BUILD_DIR)/test_parser
aether-cli: $(BUILD_DIR)/aether

test: tokenizer parser-test
	@echo "=== Tokenizer Tests ==="
	@$(BUILD_DIR)/test_tokenizer
	@echo ""
	@echo "=== Parser Tests ==="
	@$(BUILD_DIR)/test_parser

clean:
	rm -rf $(BUILD_DIR)