# Binary compression targets
# Check if UPX is available
UPX_AVAILABLE := $(shell command -v upx >/dev/null 2>&1 && echo yes || echo no)

ifeq ($(UPX_AVAILABLE),yes)
    COMPRESS_TOOL ?= upx
    COMPRESS_FLAGS ?= --best
else
    # Fallback to no compression if UPX not available
    COMPRESS_TOOL = echo
    COMPRESS_FLAGS = "UPX not available, skipping compression"
endif

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
ifeq ($(PLATFORM),windows)
	@mkdir $@ 2>nul || echo.
else
	@mkdir -p $@
endif

# Debug flag
$(FLAG_DIR)/debug.flag: | $(FLAG_DIR)
	@echo "$(DEBUG_FLAGS)" > $@

# Optimization flag
$(FLAG_DIR)/optimize.flag: | $(FLAG_DIR)
	@echo "$(RELEASE_FLAGS)" > $@

# Feature flags
$(FLAG_DIR)/feature_%.flag: | $(FLAG_DIR)
ifeq ($(PLATFORM),windows)
	@type nul > $@
else
	@touch $@
endif

# Flag-based target selection (check file existence)
ifneq ($(wildcard $(FLAG_DIR)/debug.flag),)
    BUILD_CFLAGS = $(DEBUG_FLAGS)
else
    BUILD_CFLAGS = $(RELEASE_FLAGS)
endif
