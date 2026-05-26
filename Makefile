# Main Makefile for 5G UE Simulation
.DEFAULT_GOAL := all

include config.mk
include rules.mk
include targets.mk
include compress.mk

# Detect operating system
ifeq ($(OS),Windows_NT)
    PLATFORM = windows
    EXE_EXT = .exe
    RM = del /Q
    MKDIR = mkdir
else
    PLATFORM = unix
    EXE_EXT = 
    RM = rm -f
    MKDIR = mkdir -p
endif

# SCTP library linking (set by configure script)
# SCTP_LDFLAGS is defined in rules.mk based on HAVE_SCTP detection
LDFLAGS += $(SCTP_LDFLAGS)

# Windows-specific libraries
ifeq ($(PLATFORM),windows)
    LDFLAGS += -lws2_32
endif

# Default target
all: $(TARGET)$(EXE_EXT)

# Mock gNB Server (main.c excluded from main build)
MOCK_GNB_SRCS = src/mock_gnb/mock_gnb_server.c src/mock_gnb/mock_gnb_response.c
MOCK_GNB_DEPS = src/protocol/asn1_per.c src/core/memory.c
MOCK_GNB_OBJS = $(MOCK_GNB_SRCS:.c=.o) $(MOCK_GNB_DEPS:.c=.o)
MOCK_GNB_TARGET = mock_gnb_server

mock-gnb: $(MOCK_GNB_TARGET)$(EXE_EXT)

$(MOCK_GNB_TARGET)$(EXE_EXT): $(MOCK_GNB_OBJS)
	$(CC) $(MOCK_GNB_OBJS) $(LDFLAGS) -o $@
	@echo "Built mock gNB server: $@"

# Mock Core Network Server (main.c excluded from main build)
MOCK_CORE_SRCS = src/mock_core/mock_core_server.c \
                  src/mock_core/mock_amf.c src/mock_core/mock_smf.c src/mock_core/mock_upf.c \
                  src/mock_core/mock_cu_cp.c src/mock_core/mock_du.c src/mock_core/mock_cu_up.c \
                  src/mock_core/mock_xnap.c
MOCK_CORE_DEPS = src/core/memory.c
MOCK_CORE_OBJS = $(MOCK_CORE_SRCS:.c=.o) $(MOCK_CORE_DEPS:.c=.o)
MOCK_CORE_TARGET = mock_core_server

mock-core: $(MOCK_CORE_TARGET)$(EXE_EXT)

$(MOCK_CORE_TARGET)$(EXE_EXT): $(MOCK_CORE_OBJS)
	$(CC) $(MOCK_CORE_OBJS) $(LDFLAGS) -o $@
	@echo "Built mock core server: $@"

# Mock core object files
src/mock_core/%.o: src/mock_core/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Mock gNB object files
src/mock_gnb/%.o: src/mock_gnb/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Protocol files for mock gNB
src/protocol/%.o: src/protocol/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Core files for mock gNB
src/core/%.o: src/core/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Main executable
$(TARGET)$(EXE_EXT): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
ifeq ($(PLATFORM),unix)
	$(STRIP_TOOL) $(STRIP_FLAGS) $@
  ifeq ($(COMPRESS),yes)
		$(COMPRESS_TOOL) $(COMPRESS_FLAGS) $@
  endif
endif

# Debug version
debug: CFLAGS = $(CFLAGS_DEBUG)
debug: LDFLAGS = $(LDFLAGS_DEBUG)
debug: $(TARGET_DEBUG)$(EXE_EXT)

$(TARGET_DEBUG)$(EXE_EXT): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

# Static library
$(STATIC_LIB): $(OBJECTS)
	$(AR) rcs $@ $^

# Shared library
$(SHARED_LIB): $(OBJECTS)
ifeq ($(PLATFORM),windows)
	$(CC) -shared -o $@ $^ $(LDFLAGS)
else
	$(CC) -shared -Wl,-soname,$(SHARED_LIB_MAJOR) \
	      -o $@ $^ $(LDFLAGS)
	ln -sf $(SHARED_LIB) $(SHARED_LIB_MAJOR)
	ln -sf $(SHARED_LIB_MAJOR) lib$(TARGET).so
endif

# Installation
install: install-bin install-conf

install-bin: $(TARGET)$(EXE_EXT)
ifeq ($(PLATFORM),windows)
	@echo "Installation not supported on Windows"
else
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(TARGET)$(EXE_EXT) $(DESTDIR)$(BINDIR)/
endif

install-conf:
ifeq ($(PLATFORM),windows)
	@echo "Installation not supported on Windows"
else
	install -d $(DESTDIR)$(ETCDIR)
	install -m 644 etc/uesim.conf $(DESTDIR)$(ETCDIR)/
endif

# Cleanup targets
clean:
ifeq ($(PLATFORM),windows)
	del /Q $(OBJECTS) $(TARGET)$(EXE_EXT) $(TARGET_DEBUG)$(EXE_EXT) $(STATIC_LIB) $(SHARED_LIB)* 2>nul || echo Done
	if exist $(DEPDIR) rmdir /S /Q $(DEPDIR)
	if exist $(PCH_DIR) rmdir /S /Q $(PCH_DIR)
else
	$(RM) $(OBJECTS) $(TARGET)$(EXE_EXT) $(TARGET_DEBUG)$(EXE_EXT) $(STATIC_LIB) $(SHARED_LIB)*
	rm -rf $(DEPDIR) $(PCH_DIR)
endif

distclean: clean
ifeq ($(PLATFORM),windows)
	if exist $(DEPS) del /Q $(DEPS)
	if exist tags del /Q tags
	if exist cscope.* del /Q cscope.*
else
	rm -f $(DEPS) tags cscope.*
endif

# Dependency management
$(DEPDIR):
ifeq ($(PLATFORM),windows)
	@mkdir $(DEPDIR) 2>nul || echo.
else
	@mkdir -p $@
endif

# Dependency files (flat naming to avoid subdirectory issues)
DEPS := $(addprefix $(DEPDIR)/,$(subst /,_,$(SOURCES:.c=.d)))

-include $(DEPS)

# Test targets
test: test-build test-pdcp test-rlc test-mac test-nas test-nas-pdu test-gnb-oai test-gnb-oran test-config test-cli test-benchmark test-runner
ifeq ($(PLATFORM),windows)
	test_build$(EXE_EXT)
	test_pdcp$(EXE_EXT)
	test_rlc$(EXE_EXT)
	test_mac$(EXE_EXT)
	test_nas$(EXE_EXT)
	test_nas_pdu$(EXE_EXT)
	test_gnb_oai$(EXE_EXT)
	test_gnb_oran$(EXE_EXT)
	test_config$(EXE_EXT)
	test_cli$(EXE_EXT)
	test_benchmark$(EXE_EXT)
	test_runner$(EXE_EXT)
else
	./test_build
	./test_pdcp
	./test_rlc
	./test_mac
	./test_nas
	./test_nas_pdu
	./test_gnb_oai
	./test_gnb_oran
	./test_config
	./test_cli
	./test_benchmark
	./test_runner
endif

test-build: $(TEST_OBJECTS)
	$(CC) $(TEST_OBJECTS) $(LDFLAGS) -o test_build$(EXE_EXT)

test-pdcp: tests/test_pdcp.c
	$(CC) $(CFLAGS) tests/test_pdcp.c src/protocol/pdcp.c src/protocol/snow3g.c src/protocol/aes.c src/protocol/zuc.c src/core/memory.c -o test_pdcp$(EXE_EXT) $(LDFLAGS)

test-rlc: tests/test_rlc.c
	$(CC) $(CFLAGS) tests/test_rlc.c src/protocol/rlc.c src/protocol/pdcp.c src/core/memory.c -o test_rlc$(EXE_EXT) $(LDFLAGS)

test-mac: tests/test_mac.c
	$(CC) $(CFLAGS) tests/test_mac.c src/protocol/mac.c src/protocol/rlc.c src/protocol/pdcp.c src/core/memory.c -o test_mac$(EXE_EXT) $(LDFLAGS)

test-nas: tests/test_nas.c
	$(CC) $(CFLAGS) tests/test_nas.c src/nas/nas.c src/protocol/mac.c src/protocol/rlc.c src/protocol/pdcp.c src/core/memory.c -o test_nas$(EXE_EXT) $(LDFLAGS)

test-nas-pdu: tests/test_nas_pdu.c
	$(CC) $(CFLAGS) tests/test_nas_pdu.c src/nas/nas.c src/core/memory.c -o test_nas_pdu$(EXE_EXT) $(LDFLAGS)

test-gnb-oai: tests/test_gnb_oai.c
	$(CC) $(CFLAGS) tests/test_gnb_oai.c src/protocol/rrc.c src/transport/socket_mgr.c \
	      src/core/memory.c -o test_gnb_oai$(EXE_EXT) $(LDFLAGS)

test-gnb-oran: tests/test_gnb_oran.c
	$(CC) $(CFLAGS) tests/test_gnb_oran.c src/protocol/rrc.c src/transport/socket_mgr.c \
	      src/core/memory.c -o test_gnb_oran$(EXE_EXT) $(LDFLAGS)

test-config: tests/test_config.c
	$(CC) $(CFLAGS) tests/test_config.c src/config/config.c src/core/memory.c -o test_config$(EXE_EXT) $(LDFLAGS)

test-cli: tests/test_cli.c
	$(CC) $(CFLAGS) tests/test_cli.c src/cli/cli.c src/config/config.c src/core/memory.c -o test_cli$(EXE_EXT) $(LDFLAGS)

test-benchmark: tests/test_benchmark.c
	$(CC) $(CFLAGS) tests/test_benchmark.c src/benchmark/benchmark.c src/core/memory.c -o test_benchmark$(EXE_EXT) $(LDFLAGS)

test-runner: tests/test_runner.c
	$(CC) $(CFLAGS) tests/test_runner.c src/core/memory.c -o test_runner$(EXE_EXT) $(LDFLAGS)

# Integration test with mock components
test-integration: tests/test_integration_flow.c
	$(CC) $(CFLAGS) tests/test_integration_flow.c \
	      src/mock_integration/mock_test_env.c \
	      src/mock_integration/test_flow_controller.c \
	      src/mock_core/mock_amf.c \
	      src/mock_core/mock_smf.c \
	      src/mock_core/mock_upf.c \
	      src/mock_core/mock_cu_cp.c \
	      src/mock_core/mock_du.c \
	      src/mock_core/mock_cu_up.c \
	      src/mock_core/mock_xnap.c \
	      src/core/memory.c \
	      -o test_integration_flow$(EXE_EXT) $(LDFLAGS)
	@echo "Running integration tests..."
ifeq ($(PLATFORM),windows)
	test_integration_flow$(EXE_EXT)
else
	./test_integration_flow
endif

# Test mode run (uses uesim with -t flag)
test-mode-run: $(TARGET)$(EXE_EXT)
ifeq ($(PLATFORM),windows)
	$(TARGET)$(EXE_EXT) -t -u 3
else
	./$(TARGET) -t -u 3
endif

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
	@echo "Mock Server Targets:"
	@echo "  mock-core    - Build mock core network server"
	@echo "  mock-gnb     - Build mock gNB server"
	@echo ""
	@echo "Test Mode:"
	@echo "  test-mode-run    - Run UESim in test mode"
	@echo "  test-integration - Run integration tests"
	@echo ""
	@echo "Options:"
	@echo "  BUILD_TYPE=debug|release|profile"
	@echo "  COMPRESS=yes|no"
	@echo "  ENABLE_TESTS=yes|no"
	@echo "  CROSS_COMPILE=yes|no CROSS_COMPILER=arm-linux-gnueabihf"
	@echo ""
	@echo "Platform: $(PLATFORM)"
	@echo "Executable extension: $(EXE_EXT)"