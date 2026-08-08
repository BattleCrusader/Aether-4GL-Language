# Aether Compiler — Makefile
#
# The C bootstrap compiler and C transpiler were discarded (commit 803c122).
# The compiler is now a Go bootstrap tool (cmd/bootstrap) that emits native
# ELF64/Mach-O binaries directly. This Makefile wraps the Go build/test
# workflow.

# Host is Apple Silicon (ARM64); the bootstrap emits x86_64 by default, so
# force GOARCH=amd64 for the host build. Override with `make GOARCH=arm64`.
GOARCH ?= amd64
GOFLAGS ?=

# Bootstrap compiler source
BOOTSTRAP_DIR = cmd/bootstrap
BOOTSTRAP_SRCS = $(wildcard $(BOOTSTRAP_DIR)/*.go)

# Output binary (build/ — NOT repo root, which has the aether/ source dir)
BUILD_DIR = build
AETHER = $(BUILD_DIR)/aether

# Test fixtures (compiled by the bootstrap compiler)
TEST_DIR = tests
FIXTURE_DIR = $(TEST_DIR)/fixtures
TEST_FIXTURES = $(wildcard $(FIXTURE_DIR)/*.ae)

# Installation paths
PREFIX      ?= /usr/local
BINDIR      ?= $(PREFIX)/bin
LOCAL_PREFIX ?= $(HOME)/.local
LOCAL_BINDIR ?= $(LOCAL_PREFIX)/bin

.PHONY: all build self-host aether-cli test test-host test-spec test-negative clean install install-local uninstall

all: build

# Build the Go bootstrap compiler -> build/aether
build: $(AETHER)

$(AETHER): $(BOOTSTRAP_SRCS)
	@mkdir -p $(BUILD_DIR)
	cd $(BOOTSTRAP_DIR) && GOARCH=$(GOARCH) go build $(GOFLAGS) -o ../../$(AETHER) .

# Self-hosted Aether compiler: run the bootstrap against the Aether compiler
# source (aether/*.ae) to produce build/aether_v2. This is the Phase 4 goal —
# the Aether compiler written in Aether, compiled by the bootstrap.
AETHER_SRCS = $(wildcard aether/*.ae)
AETHER_V2 = $(BUILD_DIR)/aether_v2

self-host: build
	@echo "=== Self-hosting: compiling Aether compiler source with ./$(AETHER) ==="
	@mkdir -p $(BUILD_DIR)
	./$(AETHER) $(AETHER_SRCS) -o $(AETHER_V2)
	@echo "  -> $(AETHER_V2)"
	@file $(AETHER_V2)

aether-cli: build

# Run the Go bootstrap compiler's unit test suite (86 tests)
test: build
	cd $(BOOTSTRAP_DIR) && GOARCH=$(GOARCH) go test -v -count=1 -timeout 30s

# Compile every .ae fixture with the bootstrap compiler (compile-check only)
test-host: build
	@echo "=== Compiling $(words $(TEST_FIXTURES)) fixtures with ./$(AETHER) ==="
	@total=0; pass=0; fail=0; \
	for fixture in $(TEST_FIXTURES); do \
		total=$$((total + 1)); \
		name=$$(basename $$fixture .ae); \
		out="/tmp/aether_fixture_$$name"; \
		printf "  COMPILE: %-40s " $$name; \
		if ./$(AETHER) $$fixture -o $$out >/dev/null 2>&1; then \
			printf "OK\n"; pass=$$((pass + 1)); \
		else \
			printf "FAIL\n"; fail=$$((fail + 1)); \
		fi; \
		rm -f $$out; \
	done; \
	echo ""; \
	echo "=== Results: $$pass/$$total passed, $$fail failed ==="; \
	[ $$fail -eq 0 ]

# Run the comprehensive spec test suite (positive + negative)
# Uses tools/test_runner.py which handles both kinds of fixtures.
# Positive tests must COMPILE (TDD: features being implemented).
# Negative tests must FAIL TO COMPILE (TDD: compiler must reject invalid programs).
test-spec: build
	@python3 tools/test_runner.py ./$(AETHER) $(FIXTURE_DIR)

# Run only the negative tests (compiler must reject invalid programs).
test-negative: build
	@echo "=== Running negative test suite (compiler must REJECT these) ==="
	@total=0; pass=0; fail=0; \
	for fixture in $(FIXTURE_DIR)/negative/*.ae; do \
		[ -f "$$fixture" ] || continue; \
		total=$$((total + 1)); \
		name=$$(basename $$fixture .ae); \
		out="/tmp/aether_neg_$$name"; \
		printf "  REJECT: %-50s " $$name; \
		if ./$(AETHER) $$fixture -o $$out >/dev/null 2>&1; then \
			printf "WRONG! (compiler accepted invalid code)\n"; \
			fail=$$((fail + 1)); \
		else \
			printf "OK (correctly rejected)\n"; \
			pass=$$((pass + 1)); \
		fi; \
		rm -f $$out; \
	done; \
	echo ""; \
	echo "=== Results: $$pass/$$total correctly rejected, $$fail wrongly accepted ==="; \
	[ $$fail -eq 0 ]

# Install the compiler binary to the system
install: build
	@echo "Installing Aether compiler..."
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(AETHER) $(DESTDIR)$(BINDIR)/aether
	@echo "  -> $(DESTDIR)$(BINDIR)/aether"
	@echo ""
	@echo "Aether compiler installed successfully."
	@echo "  Binary:  $(DESTDIR)$(BINDIR)/aether"
	@echo "  To use:  aether --help"
	@echo "  To compile: aether build source.ae"

# Install locally to ~/.local (no sudo needed)
install-local: build
	@echo "Installing Aether compiler locally..."
	install -d $(LOCAL_BINDIR)
	install -m 755 $(AETHER) $(LOCAL_BINDIR)/aether
	@echo "  -> $(LOCAL_BINDIR)/aether"
	@echo ""
	@echo "Aether compiler installed locally."
	@echo "  Binary:  $(LOCAL_BINDIR)/aether"
	@echo "  Make sure $(LOCAL_BINDIR) is in your PATH."

# Uninstall the aether compiler
uninstall:
	@echo "Removing Aether compiler..."
	rm -f $(DESTDIR)$(BINDIR)/aether
	@echo "  -> $(DESTDIR)$(BINDIR)/aether (removed)"
	@echo ""
	@echo "Aether compiler uninstalled."

clean:
	rm -f $(AETHER)
	rm -rf build
