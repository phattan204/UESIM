# Main targets
TARGET = uesim
TARGET_DEBUG = $(TARGET)-debug
TARGET_PROFILE = $(TARGET)-profile

# Source directories (exclude main.c files from mock_core and mock_gnb - they have their own targets)
SRCDIRS = src src/core src/protocol src/transport src/cli src/config src/nas src/benchmark src/utils src/mock_core src/mock_gnb src/mock_integration
ALL_SOURCES = $(foreach dir,$(SRCDIRS),$(wildcard $(dir)/*.c))
# Exclude main.c files from mock_core and mock_gnb directories
SOURCES = $(filter-out src/mock_core/main.c src/mock_gnb/main.c,$(ALL_SOURCES))
OBJECTS = $(SOURCES:.c=.o)
DEPS = $(SOURCES:.c=.d)

# Test directories
TESTDIRS = tests
TEST_SOURCES = $(foreach dir,$(TESTDIRS),$(wildcard $(dir)/*.c))
TEST_OBJECTS = $(TEST_SOURCES:.c=.o)
TEST_TARGETS = test-build

# Library targets
STATIC_LIB = lib$(TARGET).a
SHARED_LIB = lib$(TARGET).so.$(VERSION)
SHARED_LIB_MAJOR = lib$(TARGET).so.1

# Installation targets
INSTALL_TARGETS = install-bin install-conf install-man
UNINSTALL_TARGETS = uninstall-bin uninstall-conf uninstall-man
