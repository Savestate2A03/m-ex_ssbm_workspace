# DEVKITPPC (GCC Toolchain)
DEVKITPPC ?= /opt/devkitpro/devkitPPC
PREFIX := $(DEVKITPPC)/bin/powerpc-eabi-

AS := $(PREFIX)as
CC := $(PREFIX)gcc
LD := $(PREFIX)ld
OBJCOPY := $(PREFIX)objcopy
OBJDUMP := $(PREFIX)objdump

# Look for Python
PYTHON := $(shell command -v python3 || command -v python || command -v py || echo "")

# Directories
BUILD_DIR_BASE := ./build
BUILD_DIR := $(BUILD_DIR_BASE)/src
OUTPUT_DIR := $(BUILD_DIR_BASE)/output
SYSTEM_BUILD_DIR := $(BUILD_DIR_BASE)/system
SRC_DIR := ./src
LINKS_DIR := ./m-ex/MexTK/links

HEADERS := $(shell find ./include -name '*.h')

# Includes
INCLUDE_DIR := -I./m-ex/MexTK/include -I./include/system -I./include/user

# Outputs
OUTPUT_ELF := $(OUTPUT_DIR)/inject.elf
OUTPUT_MAP := $(OUTPUT_DIR)/inject.map
OUTPUT_BIN := $(OUTPUT_DIR)/inject.bin
OUTPUT_ASM := $(OUTPUT_DIR)/inject.asm
OUTPUT_ASM_HEX := $(OUTPUT_DIR)/inject_hex.asm

# Flags
CFLAGS := -DGEKKO -mogc -mcpu=750         # Target Gekko
CFLAGS += -meabi                          # Embedded ABI calling convention
CFLAGS += -mhard-float                    # Hardware floating point
CFLAGS += -O2                             # Optimization lvl 2
CFLAGS += -mno-sdata                      # Disable "small data area optimization" (Don't use r2/r13)
CFLAGS += -G0                             # Small data threshold to 0 bytes (Just in case...)
CFLAGS += -std=c23                        # C23 standard
CFLAGS += -fPIC                           # Position-independent code
CFLAGS += -fno-asynchronous-unwind-tables # No .eh_frame_hdr (exception handling metadata section)
CFLAGS += -fno-unwind-tables              # No .eh_frame (stack unwinding section)
CFLAGS += -fno-zero-initialized-in-bss    # Zero-init globals in .data (not .bss)
CFLAGS += -ffreestanding                  # No standard library
CFLAGS += -fno-hosted                     # Bare metal
CFLAGS += $(CFLAGS_EXTRAS) $(INCLUDE_DIR)

LDFLAGS := -Map=$(OUTPUT_MAP) -nostdlib -e _patch
OBJDUMP_FLAGS := -D --disassemble-zeroes --insn-width=4
OBJDUMP_FLAGS_HEX := -s --disassemble-zeroes

# Sources
LINKER_SCRIPT := $(BUILD_DIR)/melee.ld
ALL_OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(shell find $(SRC_DIR) -name '*.c'))

# Targets
all: $(OUTPUT_BIN) $(OUTPUT_ASM) $(OUTPUT_ASM_HEX) hex

$(BUILD_DIR) $(OUTPUT_DIR):
	@mkdir -p $@

$(SYSTEM_BUILD_DIR):
	@mkdir -p $@

# Generates the Melee linker script using melee.link from m-ex
$(LINKER_SCRIPT): $(LINKS_DIR)/melee.link | $(BUILD_DIR)
	@echo "Generating linker script..."
	@bash ./generate_linker_script.sh $< $@

# C
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(HEADERS) | $(BUILD_DIR)
	@echo "Compiling: $<"
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) -MMD -MP $< -o $@

-include $(ALL_OBJECTS:.o=.d)

# Link
$(OUTPUT_ELF): $(ALL_OBJECTS) $(LINKER_SCRIPT) | $(OUTPUT_DIR)
	@echo "Linking..."
	$(LD) $(ALL_OBJECTS) -o $@ -T $(LINKER_SCRIPT) $(LDFLAGS)

# Binary file
$(OUTPUT_BIN): $(OUTPUT_ELF)
	@echo "Creating binary..."
	$(OBJCOPY) -O binary --only-section=.text $< $@
	@echo "Binary size: $$(stat -c%s $@ 2>/dev/null || wc -c < $@) bytes"

# Disasm
$(OUTPUT_ASM): $(OUTPUT_ELF)
	@echo "Creating disassembly..."
	$(OBJDUMP) $(OBJDUMP_FLAGS) $< > $@

$(OUTPUT_ASM_HEX): $(OUTPUT_ELF)
	@echo "Creating hex disassembly..."
	$(OBJDUMP) $(OBJDUMP_FLAGS_HEX) $< > $@

# Extract from disasm for use in (for example) Melee Code Manager
hex: $(OUTPUT_ASM) $(OUTPUT_BIN)
	@if [ -n "$(PYTHON)" ]; then \
		$(PYTHON) extract_hex.py $(OUTPUT_ASM) $(OUTPUT_ASM_HEX); \
	else \
		echo "Error: Python not found."; \
		exit 1; \
	fi

clean:
	@echo "Cleaning build directory..."
	@rm -rf $(BUILD_DIR_BASE)

rebuild: clean all

.PHONY: all hex clean rebuild