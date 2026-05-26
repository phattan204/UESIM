# Dependency tracking
DEPDIR = .deps

# Use flat dep file names (replace / with _ to avoid subdirectory issues)
DEP_BASE = $(subst /,_,$*)
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$(DEP_BASE).Td

# SCTP support (from configure detection)
# HAVE_SCTP is set by configure script (1 = enabled, 0 = disabled)
ifeq ($(HAVE_SCTP),1)
    SCTP_CFLAGS += -DHAVE_SCTP
    SCTP_LDFLAGS = -lsctp
else
    SCTP_CFLAGS =
    SCTP_LDFLAGS =
endif

# Compiler warnings for code quality
# Detect unused functions, variables, and potential issues
WARNINGS = -Wall -Wextra
WARNINGS += -Wunused-function
WARNINGS += -Wunused-variable
WARNINGS += -Wunused-parameter
WARNINGS += -Wmissing-prototypes
WARNINGS += -Wstrict-prototypes
WARNINGS += -Wold-style-definition
# Suppress warnings for intentionally unused static functions
WARNINGS += -Wno-unused-function

# Compilation rule (with SCTP flags and warnings)
COMPILE.c = $(CC) $(DEPFLAGS) $(CFLAGS) $(CPPFLAGS) $(SCTP_CFLAGS) $(WARNINGS) -c

# Platform-specific post-compile
ifeq ($(PLATFORM),windows)
POSTCOMPILE = @move /Y $(DEPDIR)\$(DEP_BASE).Td $(DEPDIR)\$(DEP_BASE).d >nul 2>&1
else
POSTCOMPILE = @mv -f $(DEPDIR)/$(DEP_BASE).Td $(DEPDIR)/$(DEP_BASE).d && touch $@
endif

# Simple pattern rule: .c -> .o with auto-dependency generation
%.o: %.c | $(DEPDIR)
ifeq ($(PLATFORM),windows)
	@-mkdir $(@D) 2>nul
else
	@mkdir -p $(@D)
endif
	$(COMPILE.c) $(OUTPUT_OPTION) $<
	$(POSTCOMPILE)

# Precompiled headers (for ASN.1 structures)
PCH_DIR = include/pch
PCH_FLAGS = -include $(PCH_DIR)/protocol.h

# Conditional compilation
ifeq ($(ENABLE_TESTS),yes)
    TEST_CFLAGS = -DENABLE_UNIT_TESTS
    TEST_SOURCES = $(wildcard tests/*.c)
    TEST_OBJECTS = $(TEST_SOURCES:.c=.o)
endif

# Cross-compilation support
ifeq ($(CROSS_COMPILE),yes)
    CC = $(CROSS_COMPILER)-gcc
    STRIP_TOOL = $(CROSS_COMPILER)-strip
endif