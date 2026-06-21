HOST_CC = gcc
HOST_CFLAGS = -std=c11 -Wall -Wextra -g -O0 -Iinclude -D_GNU_SOURCE \
	-DLD='"x86_64-elf-ld"'

BUILD_DIR = build
SRC_DIR = src
TEST_DIR = tests

# Installation paths
PREFIX     ?= /usr/local
BINDIR     ?= $(PREFIX)/bin
LIBDIR     ?= $(PREFIX)/lib/aether
MANDIR     ?= $(PREFIX)/share/man/man1

# Local install paths (no sudo needed)
LOCAL_PREFIX ?= $(HOME)/.local
LOCAL_BINDIR ?= $(LOCAL_PREFIX)/bin
LOCAL_LIBDIR ?= $(LOCAL_PREFIX)/lib/aether

CORE_SRCS = \
	src/arena.c \
	src/vector.c \
	src/str.c \
	src/tokenizer.c \
	src/lexer.c \
	src/ast.c \
	src/parser.c \
	src/semantic.c \
	src/codegen.c \
	src/asm_ir.c \
	src/asm_parser.c \
	src/asm_backend_x86_64.c \
	src/asm_backend_arm64.c \
	src/asm_backend_riscv64.c \
	src/universal.c \
	src/optimizer.c

CORE_OBJS = $(CORE_SRCS:src/%.c=$(BUILD_DIR)/%.o)

.PHONY: all clean test tokenizer parser-test aether-cli install uninstall install-local

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

$(BUILD_DIR)/test_asm: $(CORE_OBJS) $(BUILD_DIR)/test_asm.o
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

test: tokenizer parser-test $(BUILD_DIR)/test_asm
	@echo "=== Tokenizer Tests ==="
	@$(BUILD_DIR)/test_tokenizer
	@echo ""
	@echo "=== Parser Tests ==="
	@$(BUILD_DIR)/test_parser
	@echo ""
	@echo "=== ASM Tests ==="
	@$(BUILD_DIR)/test_asm

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
	tests/fixtures/test_optional.ae \
	tests/fixtures/test_trait.ae \
	tests/fixtures/test_generic.ae \
	tests/fixtures/test_iflet.ae \
	tests/fixtures/test_trycatch.ae \
	tests/fixtures/test_throw.ae \
	tests/fixtures/test_errors.ae \
	tests/fixtures/test_comptime.ae \
	tests/fixtures/test_const.ae \
	tests/fixtures/test_contract.ae \
	tests/fixtures/test_closure.ae \
	tests/fixtures/test_op_overload.ae \
	tests/fixtures/test_monomorph.ae \
	tests/fixtures/test_dyn.ae \
	tests/fixtures/test_sysfunc.ae \
	tests/fixtures/test_export.ae \
	tests/fixtures/test_entry.ae \
	tests/fixtures/test_module_abi.ae \
	tests/fixtures/test_interp_basic.ae \
	tests/fixtures/test_interp_multi.ae \
	tests/fixtures/test_interp_expr.ae \
	tests/fixtures/test_interp_none.ae \
	tests/fixtures/test_interp_concat.ae \
	tests/fixtures/test_interp_numbers.ae \
	tests/fixtures/test_interp_numeric.ae \
	tests/fixtures/test_interp_num_concat.ae \
	tests/fixtures/test_interp_print_num.ae \
	tests/fixtures/test_import.ae

# Expected exit codes for each fixture
TEST_EXPECTED = 42 165 150 200 0 0 30 42 0 0 0 42 42 42 42 42 42 128 42 42 42 42 42 42 42 42 42 42 42 42 42 42 42 42 42 42 42

# Layout test fixtures — compiled as flat binary, verified by size
LAYOUT_FIXTURES = \
	tests/fixtures/test_layout.ae

LAYOUT_EXPECTED_SIZES = 512

# New-target test fixtures — compile check only (no native run)
NEW_TARGET_FIXTURES = \
	tests/fixtures/test_kernel.ae \
	tests/fixtures/test_module_target.ae \
	tests/fixtures/test_binary_target.ae \
	tests/fixtures/test_boot_target.ae

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
			printf "FAIL (compile)\n"; \
			continue; \
		fi; \
		/tmp/$$name 2>/dev/null >/dev/null; \
		got=$$?; \
		if [ "$$got" = "$$expected" ]; then \
			printf "PASS (exit %d)\n" $$got; \
			pass=$$((pass + 1)); \
		else \
			printf "FAIL (expected %d, got %d)\n" $$expected $$got; \
		fi; \
	done; \
	echo ""; \
	echo "=== Results: $$pass/$$total passed, $$((total - pass)) failed ==="; \
	[ $$pass -eq $$total ]

test-layout: aether-cli
	@echo "=== Layout (Flat Binary) Test Runner ==="
	@echo ""
	@total=0; pass=0; \
	set -- $(LAYOUT_EXPECTED_SIZES); \
	for fixture in $(LAYOUT_FIXTURES); do \
		total=$$((total + 1)); \
		expected=$$1; shift; \
		name=$$(basename $$fixture .ae); \
		out="/tmp/$$name.bin"; \
		printf "  TEST: %s ... " $$name; \
		./$(BUILD_DIR)/aether --target x86_64-freestanding $$fixture -o $$out 2>/dev/null >/dev/null; \
		if [ $$? -ne 0 ]; then \
			printf "FAIL (compile)\n"; \
			continue; \
		fi; \
		actual=$$(stat -f%z $$out 2>/dev/null || stat -c%s $$out 2>/dev/null); \
		if [ "$$actual" = "$$expected" ]; then \
			printf "PASS (%d bytes)\n" $$actual; \
			pass=$$((pass + 1)); \
		else \
			printf "FAIL (expected %d bytes, got %d)\n" $$expected $$actual; \
		fi; \
	done; \
	echo ""; \
	echo "=== Results: $$pass/$$total passed, $$((total - pass)) failed ==="; \
	[ $$pass -eq $$total ]

# Install the aether compiler and standard library to the system
install: aether-cli
	@echo "Installing Aether compiler..."
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(BUILD_DIR)/aether $(DESTDIR)$(BINDIR)/aether
	@echo "  -> $(DESTDIR)$(BINDIR)/aether"
	@echo "Installing standard library..."
	install -d $(DESTDIR)$(LIBDIR)
	for f in std/*.ae; do \
		install -m 644 $$f $(DESTDIR)$(LIBDIR)/$$(basename $$f); \
		echo "  -> $(DESTDIR)$(LIBDIR)/$$(basename $$f)"; \
	done
	@echo "Installing header files..."
	install -d $(DESTDIR)$(LIBDIR)/include
	for f in include/aether/*.h; do \
		install -m 644 $$f $(DESTDIR)$(LIBDIR)/include/$$(basename $$f); \
		echo "  -> $(DESTDIR)$(LIBDIR)/include/$$(basename $$f)"; \
	done
	@echo ""
	@echo "Aether compiler installed successfully."
	@echo "  Binary:  $(DESTDIR)$(BINDIR)/aether"
	@echo "  Stdlib:  $(DESTDIR)$(LIBDIR)/"
	@echo ""
	@echo "To use: aether --help"
	@echo "To compile: aether build source.ae"
	@echo "To run:    aether run source.ae"

# Install locally to ~/.local (no sudo needed)
install-local: aether-cli
	@echo "Installing Aether compiler locally..."
	install -d $(LOCAL_BINDIR)
	install -m 755 $(BUILD_DIR)/aether $(LOCAL_BINDIR)/aether
	@echo "  -> $(LOCAL_BINDIR)/aether"
	@echo "Installing standard library..."
	install -d $(LOCAL_LIBDIR)
	for f in std/*.ae; do \
		install -m 644 $$f $(LOCAL_LIBDIR)/$$(basename $$f); \
		echo "  -> $(LOCAL_LIBDIR)/$$(basename $$f)"; \
	done
	@echo "Installing header files..."
	install -d $(LOCAL_LIBDIR)/include
	for f in include/aether/*.h; do \
		install -m 644 $$f $(LOCAL_LIBDIR)/include/$$(basename $$f); \
		echo "  -> $(LOCAL_LIBDIR)/include/$$(basename $$f)"; \
	done
	@echo ""
	@echo "Aether compiler installed locally."
	@echo "  Binary:  $(LOCAL_BINDIR)/aether"
	@echo "  Stdlib:  $(LOCAL_LIBDIR)/"
	@echo ""
	@echo "Make sure $(LOCAL_BINDIR) is in your PATH."
	@echo "To use: aether --help"

# Uninstall the aether compiler and standard library
uninstall:
	@echo "Removing Aether compiler..."
	rm -f $(DESTDIR)$(BINDIR)/aether
	@echo "  -> $(DESTDIR)$(BINDIR)/aether (removed)"
	@echo "Removing standard library..."
	rm -rf $(DESTDIR)$(LIBDIR)
	@echo "  -> $(DESTDIR)$(LIBDIR)/ (removed)"
	@echo ""
	@echo "Aether compiler uninstalled."

clean:
	rm -rf $(BUILD_DIR)
