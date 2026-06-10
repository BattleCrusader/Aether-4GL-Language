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

# Host-native test runner — compiles .ae fixtures and runs them natively
TEST_FIXTURES = \
	tests/fixtures/hello.ae \
	tests/fixtures/test_math.ae \
	tests/fixtures/test_params.ae \
	tests/fixtures/test_types.ae \
	tests/fixtures/test_struct.ae \
	tests/fixtures/test_enum.ae \
	tests/fixtures/test_match.ae \
	tests/fixtures/test_defer.ae \
	tests/fixtures/test_region.ae \
	tests/fixtures/test_optional.ae

# Expected exit codes for each fixture
TEST_EXPECTED = 42 165 150 200 0 0 30 42 0 0

test-host: aether-cli
	@echo "=== Host-Native Test Runner ==="
	@echo ""
	@total=0; pass=0; \
	set -- $(TEST_EXPECTED); \
	for fixture in $(TEST_FIXTURES); do \
		total=$$((total + 1)); \
		expected=$$1; shift; \
		name=$$(basename $$fixture .ae); \
		printf "  TEST: %s ... " $$name; \
		./$(BUILD_DIR)/aether --target host $$fixture 2>/dev/null >/dev/null; \
		if [ $$? -ne 0 ]; then \
			printf "FAIL (compile)\\n"; \
			continue; \
		fi; \
		/tmp/$$name 2>/dev/null >/dev/null; \
		got=$$?; \
		if [ "$$got" = "$$expected" ]; then \
			printf "PASS (exit %d)\\n" $$got; \
			pass=$$((pass + 1)); \
		else \
			printf "FAIL (expected %d, got %d)\\n" $$expected $$got; \
		fi; \
	done; \
	echo ""; \
	echo "=== Results: $$pass/$$total passed, $$((total - pass)) failed ==="; \
	[ $$pass -eq $$total ]

clean:
	rm -rf $(BUILD_DIR)