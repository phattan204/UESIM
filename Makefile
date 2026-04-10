# Main Makefile for 5G UE Simulation
include config.mk
include rules.mk
include targets.mk
include compress.mk

# Default target
all: $(TARGET)

# Main executable
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(STRIP_TOOL) $(STRIP_FLAGS) $@
ifeq ($(COMPRESS),yes)
	$(COMPRESS_TOOL) $(COMPRESS_FLAGS) $@
endif

# Debug version
debug: CFLAGS = $(CFLAGS_DEBUG)
debug: LDFLAGS = $(LDFLAGS_DEBUG)
debug: $(TARGET_DEBUG)

$(TARGET_DEBUG): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

# Static library
$(STATIC_LIB): $(OBJECTS)
	$(AR) rcs $@ $^

# Shared library
$(SHARED_LIB): $(OBJECTS)
	$(CC) -shared -Wl,-soname,$(SHARED_LIB_MAJOR) \
	      -o $@ $^ $(LDFLAGS)
	ln -sf $(SHARED_LIB) $(SHARED_LIB_MAJOR)
	ln -sf $(SHARED_LIB_MAJOR) lib$(TARGET).so

# Installation
install: install-bin install-conf

install-bin: $(TARGET)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/

install-conf:
	install -d $(DESTDIR)$(ETCDIR)
	install -m 644 etc/uesim.conf $(DESTDIR)$(ETCDIR)/

# Cleanup targets
clean:
	rm -f $(OBJECTS) $(TARGET) $(TARGET_DEBUG) $(STATIC_LIB) $(SHARED_LIB)*
	rm -rf $(DEPDIR) $(PCH_DIR)

distclean: clean
	rm -f $(DEPS) tags cscope.*

# Dependency management
$(DEPDIR):
	@mkdir -p $@

$(DEPS): | $(DEPDIR)

-include $(DEPS)

# Test targets
test: test-build test-pdcp test-rlc test-mac test-nas test-config test-cli test-benchmark test-runner
	./test_build
	./test_pdcp
	./test_rlc
	./test_mac
	./test_nas
	./test_config
	./test_cli
	./test_benchmark
	./test_runner

test-build: $(TEST_OBJECTS)
	$(CC) $(TEST_OBJECTS) $(LDFLAGS) -o test_build

test-pdcp: tests/test_pdcp.c
	$(CC) $(CFLAGS) tests/test_pdcp.c src/protocol/pdcp.c src/protocol/snow3g.c src/protocol/aes.c src/protocol/zuc.c src/core/memory.c -o test_pdcp $(LDFLAGS)

test-rlc: tests/test_rlc.c
	$(CC) $(CFLAGS) tests/test_rlc.c src/protocol/rlc.c src/protocol/pdcp.c src/core/memory.c -o test_rlc $(LDFLAGS)

test-mac: tests/test_mac.c
	$(CC) $(CFLAGS) tests/test_mac.c src/protocol/mac.c src/protocol/rlc.c src/protocol/pdcp.c src/core/memory.c -o test_mac $(LDFLAGS)

test-nas: tests/test_nas.c
	$(CC) $(CFLAGS) tests/test_nas.c src/nas/nas.c src/protocol/mac.c src/protocol/rlc.c src/protocol/pdcp.c src/core/memory.c -o test_nas $(LDFLAGS)

test-config: tests/test_config.c
	$(CC) $(CFLAGS) tests/test_config.c src/config/config.c src/core/memory.c -o test_config $(LDFLAGS)

test-cli: tests/test_cli.c
	$(CC) $(CFLAGS) tests/test_cli.c src/cli/cli.c src/config/config.c src/core/memory.c -o test_cli $(LDFLAGS)

test-benchmark: tests/test_benchmark.c
	$(CC) $(CFLAGS) tests/test_benchmark.c src/benchmark/benchmark.c src/core/memory.c -o test_benchmark $(LDFLAGS)

test-runner: tests/test_runner.c
	$(CC) $(CFLAGS) tests/test_runner.c src/core/memory.c -o test_runner $(LDFLAGS)

# Help target
help:
	@echo "Usage: make [TARGET] [OPTIONS]"
	@echo ""
	@echo "Targets:"
	@echo "  all          - Build main executable (default)"
	@echo "  debug        - Build debug version"
	@echo "  profile      - Build profile-guided version"
	@echo "  static       - Build static library"
	@echo "  shared       - Build shared library"
	@echo "  test         - Build and run tests"
	@echo "  test-build   - Build test executable"
	@echo "  install      - Install to system"
	@echo "  clean        - Remove build artifacts"
	@echo "  distclean    - Remove all generated files"
	@echo ""
	@echo "Options:"
	@echo "  BUILD_TYPE=debug|release|profile"
	@echo "  COMPRESS=yes|no"
	@echo "  ENABLE_TESTS=yes|no"
	@echo "  CROSS_COMPILE=yes|no CROSS_COMPILER=arm-linux-gnueabihf"
