# Binary compression targets
COMPRESS_TOOL ?= upx
COMPRESS_FLAGS ?= --best --ultra-brute

# Stripping and optimization
STRIP_TOOL = strip
STRIP_FLAGS = --strip-all --remove-section=.comment --remove-section=.note

# Size optimization flags
SIZE_OPT_FLAGS = -ffunction-sections -fdata-sections
LINKER_GC_FLAGS = -Wl,--gc-sections -Wl,--print-gc-sections

# Profile-guided optimization (if supported)
PGO_FLAGS = -fprofile-generate -fprofile-dir=./pgo-data
PGO_USE_FLAGS = -fprofile-use -fprofile-dir=./pgo-data

# Flag files for conditional compilation
FLAG_DIR = .build_flags

# Create flag files based on configuration
$(FLAG_DIR):
	@mkdir -p $@

# Debug flag
$(FLAG_DIR)/debug.flag: | $(FLAG_DIR)
	@echo "$(DEBUG_FLAGS)" > $@

# Optimization flag
$(FLAG_DIR)/optimize.flag: | $(FLAG_DIR)
	@echo "$(RELEASE_FLAGS)" > $@

# Feature flags
$(FLAG_DIR)/feature_%.flag: | $(FLAG_DIR)
	@touch $@

# Conditional compilation based on flags
-include $(wildcard $(FLAG_DIR)/*.flag)

# Flag-based target selection
ifeq ($(wildcard $(FLAG_DIR)/debug.flag),)
    BUILD_CFLAGS = $(RELEASE_FLAGS)
else
    BUILD_CFLAGS = $(DEBUG_FLAGS)
endif