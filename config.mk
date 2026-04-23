# Build configuration
BUILD_TYPE ?= release
DEBUG_FLAGS = -g -DDEBUG -O0
RELEASE_FLAGS = -O2 -DNDEBUG
PROFILE_FLAGS = -pg -O2

# Detect operating system
ifeq ($(OS),Windows_NT)
    # Windows (MinGW/MSYS2)
    PLATFORM = windows
    CC = gcc
    CFLAGS_BASE = -std=c11 -Wall -Wextra -D_WIN32
    LDFLAGS_BASE = -lws2_32
    PREFIX ?= C:/uesim
    BINDIR ?= $(PREFIX)/bin
    LIBDIR ?= $(PREFIX)/lib
    ETCDIR ?= C:/uesim/etc
else
    # Unix-like systems (Linux, macOS)
    PLATFORM = unix
    CC = gcc
    CFLAGS_BASE = -std=c11 -Wall -Wextra -D_GNU_SOURCE
    
    # Installation paths
    PREFIX ?= /usr/local
    BINDIR ?= $(PREFIX)/bin
    LIBDIR ?= $(PREFIX)/lib
    ETCDIR ?= /etc/uesim

    # RHEL 8.5 specific flags
    RHEL_VERSION = $(shell rpm -q --queryformat '%{VERSION}' redhat-release 2>/dev/null || echo "8.5")

    # Architecture detection
    ARCH = $(shell uname -m)
    ifeq ($(ARCH),x86_64)
        ARCH_FLAGS = -march=x86-64 -mtune=generic
    endif
    ifeq ($(ARCH),aarch64)
        ARCH_FLAGS = -march=armv8-a
    endif

    # Security hardening
    SECURITY_FLAGS = -fstack-protector-strong -D_FORTIFY_SOURCE=2
    RELRO_FLAGS = -Wl,-z,relro,-z,now
    PIE_FLAGS = -fPIE -pie

    # Performance tuning for RHEL
    RHEL_PERF_FLAGS = -mbranch-cost=5 -freorder-blocks-algorithm=stc
    
    LDFLAGS_BASE = -lpthread -lrt
endif

# Compiler and linker flags (case-insensitive BUILD_TYPE matching)
ifneq (,$(filter $(BUILD_TYPE),debug DEBUG Debug))
  CFLAGS = $(CFLAGS_BASE) $(ARCH_FLAGS) $(SECURITY_FLAGS) $(DEBUG_FLAGS)
  LDFLAGS = $(LDFLAGS_BASE)
else ifneq (,$(filter $(BUILD_TYPE),profile PROFILE Profile))
  CFLAGS = $(CFLAGS_BASE) $(ARCH_FLAGS) $(SECURITY_FLAGS) $(PROFILE_FLAGS)
  LDFLAGS = $(LDFLAGS_BASE)
else
  CFLAGS = $(CFLAGS_BASE) $(ARCH_FLAGS) $(SECURITY_FLAGS) $(PIE_FLAGS) $(RELEASE_FLAGS)
  LDFLAGS = $(LDFLAGS_BASE) $(RELRO_FLAGS)
endif

# Version information
VERSION ?= 1.0.0

# Platform-specific adjustments
ifeq ($(PLATFORM),windows)
    # Windows-specific flags
    CFLAGS_BASE += -D_WIN32_WINNT=0x0600
endif

# Directory creation (platform-specific)
ifeq ($(PLATFORM),windows)
    MKDIR_P = if not exist
else
    MKDIR_P = mkdir -p
endif

# Output option for compilation
OUTPUT_OPTION = -o $@