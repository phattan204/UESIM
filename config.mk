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
    LDFLAGS_BASE = -lws2_32 -lpthread
    PREFIX ?= C:/uesim
    BINDIR ?= $(PREFIX)/bin
    LIBDIR ?= $(PREFIX)/lib
    ETCDIR ?= C:/uesim/etc
else
    # Unix-like systems (Linux, macOS)
    PLATFORM = unix
    CC = gcc
    CFLAGS_BASE = -std=c11 -Wall -Wextra -Werror -D_GNU_SOURCE
    
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
    
    LDFLAGS_BASE = -lpthread -lrt -lsctp -lconfig
endif

# Compiler and linker flags
CFLAGS_DEBUG = $(CFLAGS_BASE) $(DEBUG_FLAGS)
CFLAGS_RELEASE = $(CFLAGS_BASE) $(RELEASE_FLAGS)
CFLAGS = $(CFLAGS_$(BUILD_TYPE))

LDFLAGS_DEBUG = $(LDFLAGS_BASE) 
LDFLAGS_RELEASE = $(LDFLAGS_BASE)
LDFLAGS = $(LDFLAGS_$(BUILD_TYPE))

# Version information
VERSION ?= 1.0.0

# Platform-specific adjustments
ifeq ($(PLATFORM),windows)
    # Windows-specific flags
    CFLAGS_BASE += -D_WIN32_WINNT=0x0600
    # Remove Linux-specific flags for Windows
    LDFLAGS_DEBUG := $(filter-out -lasan -lrt -lsctp -lconfig,$(LDFLAGS_DEBUG))
    LDFLAGS_RELEASE := $(filter-out -lrt -lsctp -lconfig,$(LDFLAGS_RELEASE))
endif

# Directory creation
MKDIR_P = mkdir -p

# Output option for compilation
OUTPUT_OPTION = -o $@
