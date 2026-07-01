HOST_CC = gcc
HOST_CFLAGS = -std=c23 -Wall -Wextra -g -O0 -Iinclude -D_GNU_SOURCE \
	-DLD='"x86_64-elf-ld"' \
	-DSEGFAULT_HELPER='"$(CURDIR)/build/segfault_helper.o"'

# C transpiler source files
C_TRANSPILER_SRCS = \
	src/c_transpiler/c_transpiler.c \
	src/c_transpiler/c_types.c \
	src/c_transpiler/c_expr.c \
	src/c_transpiler/c_stmt.c \
	src/c_transpiler/c_func.c \
	src/c_transpiler/c_runtime.c \
	src/c_transpiler/c_string.c \
	src/c_transpiler/c_asm.c \
	src/c_transpiler/c_error.c \
	src/c_transpiler/c_contract.c \
	src/c_transpiler/c_target.c

GIT_HASH := $(shell git rev-parse --short HEAD 2>/dev/null || echo "unknown")
GIT_BRANCH := $(shell git rev-parse --abbrev-ref HEAD 2>/dev/null || echo "unknown")

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
	src/optimizer.c \
	src/aelib.c

# aether.o has main() — linked separately so test executables don't get duplicate main
AETHER_MAIN_SRC = src/aether.c

# Segfault helper — compiled separately, linked into host-native binaries
SEGFAULT_HELPER_SRC = src/segfault_helper.c
SEGFAULT_HELPER_OBJ = build/segfault_helper.o

CORE_OBJS = $(CORE_SRCS:src/%.c=$(BUILD_DIR)/%.o)
C_TRANSPILER_OBJS = $(C_TRANSPILER_SRCS:src/c_transpiler/%.c=$(BUILD_DIR)/c_transpiler/%.o)

.PHONY: all clean test tokenizer parser-test aether-cli install uninstall install-local

all: tokenizer parser-test aether-cli

# Pattern: compile src/c_transpiler/*.c to build/c_transpiler/*.o
$(BUILD_DIR)/c_transpiler/%.o: src/c_transpiler/%.c include/aether/c_transpiler.h
	@mkdir -p $(@D)
	$(HOST_CC) $(HOST_CFLAGS) -c $< -o $@

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

$(BUILD_DIR)/aether.o: $(AETHER_MAIN_SRC)
	@mkdir -p $(@D)
	$(HOST_CC) $(HOST_CFLAGS) -DGIT_HASH='"$(GIT_HASH)"' -DGIT_BRANCH='"$(GIT_BRANCH)"' -c $< -o $@

$(BUILD_DIR)/aether: $(CORE_OBJS) $(C_TRANSPILER_OBJS) $(BUILD_DIR)/aether.o $(SEGFAULT_HELPER_OBJ)
	$(HOST_CC) $(HOST_CFLAGS) -o $@ $(CORE_OBJS) $(C_TRANSPILER_OBJS) $(BUILD_DIR)/aether.o

# Segfault helper — compiled with host CC (needs libSystem for signal/backtrace)
$(SEGFAULT_HELPER_OBJ): $(SEGFAULT_HELPER_SRC)
	@mkdir -p $(@D)
	$(HOST_CC) -arch x86_64 -c $< -o $@

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
	tests/fixtures/test_properties.ae \
	tests/fixtures/test_monomorph.ae \
	tests/fixtures/test_dyn.ae \
	tests/fixtures/test_builtins.ae \
	tests/fixtures/test_destructor.ae \
	tests/fixtures/test_sysfunc.ae \
	tests/fixtures/test_export.ae \
	tests/fixtures/test_entry.ae \
	tests/fixtures/test_module_abi.ae \
	tests/fixtures/test_interp_basic.ae \
	tests/fixtures/test_interp_multi.ae \
	tests/fixtures/test_interp_expr.ae \
	tests/fixtures/test_interp_none.ae \
	tests/fixtures/test_interp_concat.ae \
	tests/fixtures/test_ownership.ae \
	tests/fixtures/test_throws.ae \
	tests/fixtures/test_generic_constraint.ae \
	tests/fixtures/test_invariants.ae \
	tests/fixtures/test_interp_numbers.ae \
	tests/fixtures/test_interp_numeric.ae \
	tests/fixtures/test_interp_num_concat.ae \
	tests/fixtures/test_interp_print_num.ae \
	tests/fixtures/test_import.ae \
	tests/fixtures/test_asm_comment.ae \
	tests/fixtures/test_segfault.ae \
	tests/fixtures/test_args.ae \
	tests/fixtures/test_null_concat.ae \
	tests/fixtures/test_logical_keywords.ae \
	tests/fixtures/test_aelib_import.ae \
	tests/fixtures/test_std_test.ae \
	tests/fixtures/test_variadic.ae \
	tests/fixtures/test_ternary.ae \
	tests/fixtures/test_type_alias.ae \
	tests/fixtures/test_type_infer.ae \
	tests/fixtures/test_type_fn.ae \
	tests/fixtures/test_type_tuple.ae \
	tests/fixtures/test_default_param.ae \
	tests/fixtures/test_extern.ae \
	tests/fixtures/test_finally.ae

# .aelib library fixtures — must be built before test-host
AELIB_FIXTURES = \
	tests/fixtures/lib_math.ae

# libaether.aelib — proper static library archive from individual .o files
LIBAETHER_SRCS = std/arch.ae std/asm.ae std/collections.ae std/elf.ae std/fs.ae std/io.ae std/math.ae std/mem.ae std/serial.ae std/str.ae std/test.ae
LIBAETHER_OBJS = $(LIBAETHER_SRCS:std/%.ae=$(BUILD_DIR)/lib/%.o)
LIBAETHER_AELIB = build/lib/libaether.aelib

# Compile each .ae to its own .o for the library
$(BUILD_DIR)/lib/%.o: std/%.ae aether-cli
	@mkdir -p $(@D) /tmp/kernel
	./$(BUILD_DIR)/aether --target lib $< -o $@
	@test -f $@ || { echo "ERROR: $@ was not created"; exit 1; }

# Archive all .o files into .aelib (static library)
$(LIBAETHER_AELIB): $(LIBAETHER_OBJS)
	@echo "=== Building libaether.aelib ==="
	@mkdir -p build/lib
	@ar rcs $@ $^
	@echo "  libaether.aelib built OK"

# .aelib library fixtures — must be built before test-host
AELIB_FIXTURES = \
	tests/fixtures/lib_math.ae

# Each .aelib fixture gets its own .o
tests/fixtures/%.aelib: tests/fixtures/%.ae aether-cli
	./$(BUILD_DIR)/aether --target lib $< -o $@ 2>/dev/null

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

test-host: aether-cli $(LIBAETHER_AELIB)
	@echo "=== Building .aelib library fixtures ==="
	@for fixture in $(AELIB_FIXTURES); do \
		name=$$(basename $$fixture .ae); \
		dir=$$(dirname $$fixture); \
		printf "  BUILD: %s ... " $$name; \
		./$(BUILD_DIR)/aether --target lib $$fixture -o $$dir/$$name.aelib 2>/dev/null >/dev/null; \
		if [ $$? -eq 0 ]; then \
			printf "OK\n"; \
		else \
			printf "FAIL\n"; \
		fi; \
	done
	@echo ""
	@echo "=== Host-Native Test Runner ==="
	@echo ""
	@python3 tools/test_host.py ./$(BUILD_DIR)/aether $(TEST_FIXTURES)

test-layout: aether-cli
	@echo "=== Layout (Flat Binary) Test Runner ==="
	@echo ""
	@total=0; pass=0; \
	set -- $(LAYOUT_EXPECTED_SIZES); \
	for fixture in $(LAYOUT_FIXTURES); do \
		total=$$((total + 1)); \
		expected=$$1; shift; \
		name=$$(basename $$fixture .ae); \
		out="/tmp/kernel/$$name.bin"; \
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
install: aether-cli $(LIBAETHER_AELIB)
	@echo "Installing Aether compiler..."
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(BUILD_DIR)/aether $(DESTDIR)$(BINDIR)/aether
	@echo "  -> $(DESTDIR)$(BINDIR)/aether"
	@echo "Installing standard library..."
	install -d $(DESTDIR)$(LIBDIR)
	install -m 644 $(LIBAETHER_AELIB) $(DESTDIR)$(LIBDIR)/libaether.aelib
	@echo "  -> $(DESTDIR)$(LIBDIR)/libaether.aelib"
	@echo ""
	@echo "Aether compiler installed successfully."
	@echo "  Binary:  $(DESTDIR)$(BINDIR)/aether"
	@echo "  Stdlib:  $(DESTDIR)$(LIBDIR)/libaether.aelib"
	@echo ""
	@echo "To use: aether --help"
	@echo "To compile: aether build source.ae"
	@echo "To run:    aether run source.ae"

# Install locally to ~/.local (no sudo needed)
install-local: aether-cli $(LIBAETHER_AELIB)
	@echo "Installing Aether compiler locally..."
	install -d $(LOCAL_BINDIR)
	install -m 755 $(BUILD_DIR)/aether $(LOCAL_BINDIR)/aether
	@echo "  -> $(LOCAL_BINDIR)/aether"
	@echo "Installing standard library..."
	install -d $(LOCAL_LIBDIR)
	install -m 644 $(LIBAETHER_AELIB) $(LOCAL_LIBDIR)/libaether.aelib
	@echo "  -> $(LOCAL_LIBDIR)/libaether.aelib"
	@echo ""
	@echo "Aether compiler installed locally."
	@echo "  Binary:  $(LOCAL_BINDIR)/aether"
	@echo "  Stdlib:  $(LOCAL_LIBDIR)/libaether.aelib"
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
	rm -rf /tmp/kernel
