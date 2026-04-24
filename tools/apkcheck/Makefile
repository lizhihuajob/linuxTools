# APKCheck - APK Signing Tool Makefile
# Copyright (c) 2024 APKCheck Contributors

.PHONY: all clean install uninstall test dist

# Compiler and flags
CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2 -fPIC
LDFLAGS ?=
INCLUDES = -I./include
LIBS = -lssl -lcrypto -lzip -lpthread

# Directories
SRC_DIR = src
INC_DIR = include
BUILD_DIR = build
BIN_DIR = $(BUILD_DIR)/bin
OBJ_DIR = $(BUILD_DIR)/obj

# Target
TARGET = $(BIN_DIR)/apkcheck

# Source files
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

# Install directories
PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/share/man/man1

# Version
VERSION = 1.0.0

# Default target
all: $(TARGET)

# Create build directories
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Compile source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Link the executable
$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)
	@echo "Build complete: $@"

# Debug build
debug: CFLAGS += -g -DDEBUG -O0
debug: clean all

# Release build with optimizations
release: CFLAGS += -DNDEBUG -O3 -march=native
release: clean all

# Static build (if libraries are available as static)
static: LDFLAGS += -static
static: LIBS = -lssl -lcrypto -lzip -lpthread -lz -ldl
static: clean all

# Clean build files
clean:
	rm -rf $(BUILD_DIR)
	rm -f *.tar.gz
	@echo "Clean complete"

# Install the binary
install: $(TARGET)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/
	@if [ -d docs/man ]; then \
		install -d $(DESTDIR)$(MANDIR); \
		install -m 644 docs/man/apkcheck.1 $(DESTDIR)$(MANDIR)/; \
	fi
	@echo "Installation complete"

# Uninstall
uninstall:
	rm -f $(DESTDIR)$(BINDIR)/apkcheck
	rm -f $(DESTDIR)$(MANDIR)/apkcheck.1
	@echo "Uninstallation complete"

# Run tests (if test directory exists)
test:
	@if [ -d test ]; then \
		cd test && ./run_tests.sh; \
	else \
		echo "No test directory found"; \
	fi

# Create distribution package
dist: clean
	mkdir -p apkcheck-$(VERSION)
	cp -r src include docs test Makefile README.md APKCheck-*.md apkcheck-$(VERSION)/ 2>/dev/null || true
	tar -czvf apkcheck-$(VERSION).tar.gz apkcheck-$(VERSION)
	rm -rf apkcheck-$(VERSION)
	@echo "Distribution package created: apkcheck-$(VERSION).tar.gz"

# Check for required libraries
check-libs:
	@echo "Checking for required libraries..."
	@pkg-config --exists libzip && echo "  libzip: OK" || echo "  libzip: NOT FOUND (install libzip-dev)"
	@pkg-config --exists openssl && echo "  openssl: OK" || echo "  openssl: NOT FOUND (install libssl-dev)"
	@echo "Note: On Debian/Ubuntu, install with: sudo apt-get install libzip-dev libssl-dev"

# Show help
help:
	@echo "APKCheck Makefile targets:"
	@echo "  all          - Build the executable (default)"
	@echo "  debug        - Build with debug symbols"
	@echo "  release      - Build with optimizations"
	@echo "  static       - Build static binary"
	@echo "  clean        - Remove build files"
	@echo "  install      - Install to system"
	@echo "  uninstall    - Remove from system"
	@echo "  test         - Run tests"
	@echo "  dist         - Create source distribution"
	@echo "  check-libs   - Check for required libraries"
	@echo "  help         - Show this help"

# Include dependencies
-include $(OBJS:.o=.d)
