# Dependency tracking
DEPDIR = .deps
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.Td

# Compilation pattern rules
COMPILE.c = $(CC) $(DEPFLAGS) $(CFLAGS) $(CPPFLAGS) -c
POSTCOMPILE = @mv -f $(DEPDIR)/$*.Td $(DEPDIR)/$*.d && touch $@

%.o : %.c
%.o : %.c $(DEPDIR)/%.d
	@$(MKDIR_P) $(@D) $(DEPDIR)
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