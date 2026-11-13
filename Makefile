# Builds the mod.
#   make       builds build/mod.elf (the mod code)
#   make nrm   also runs RecompModTool to make build/<mod_filename>.nrm
#   make clean deletes build/

BUILD_DIR := build

# On macOS, Apple's clang can't build for MIPS. Install llvm from Homebrew
# and pass its clang and ld.lld, like: make CC=/opt/homebrew/opt/llvm/bin/clang LD=...
ifeq ($(OS),Windows_NT)
    CC := clang
    LD := ld.lld
else ifneq ($(shell uname),Darwin)
    CC := clang
    LD := ld.lld
else
    CC ?= clang
    LD ?= ld.lld
endif

# Path to RecompModTool. Put it in this folder or pass MOD_TOOL=path/to/it.
ifeq ($(OS),Windows_NT)
    MOD_TOOL ?= ./RecompModTool.exe
else
    MOD_TOOL ?= ./RecompModTool
endif

TARGET   := $(BUILD_DIR)/mod.elf
LDSCRIPT := mod.ld

ARCHFLAGS := -target mips -mips2 -mabi=32 -O2 -G0 -mno-abicalls -mno-odd-spreg -mno-check-zero-division \
             -fomit-frame-pointer -ffast-math -fno-unsafe-math-optimizations -fno-builtin-memset -funsigned-char -fno-builtin-sinf -fno-builtin-cosf
WARNFLAGS := -Wall -Wextra -Wno-incompatible-library-redeclaration -Wno-unused-parameter -Wno-unknown-pragmas -Wno-unused-variable \
             -Wno-missing-braces -Wno-unsupported-floating-point-opt -Werror=section -Wno-visibility
CFLAGS   := $(ARCHFLAGS) $(WARNFLAGS) -D_LANGUAGE_C -nostdinc -ffunction-sections
CPPFLAGS := -nostdinc -DMIPS -I include
LDFLAGS  := -nostdlib -T $(LDSCRIPT) -Map $(BUILD_DIR)/mod.map --unresolved-symbols=ignore-all --emit-relocs -e 0 --no-nmagic -gc-sections

rwildcard = $(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))
getdirs = $(sort $(dir $(1)))

C_SRCS := $(call rwildcard,src,*.c)
C_OBJS := $(addprefix $(BUILD_DIR)/, $(C_SRCS:.c=.o))
C_DEPS := $(addprefix $(BUILD_DIR)/, $(C_SRCS:.c=.d))
BUILD_DIRS := $(call getdirs,$(C_OBJS))

all: $(TARGET)

$(TARGET): $(C_OBJS) $(LDSCRIPT) | $(BUILD_DIR)
	$(LD) $(C_OBJS) $(LDFLAGS) -o $@

nrm: $(TARGET)
	$(MOD_TOOL) mod.toml $(BUILD_DIR)

$(BUILD_DIR) $(BUILD_DIRS):
ifeq ($(OS),Windows_NT)
	if not exist "$(subst /,\,$@)" mkdir "$(subst /,\,$@)"
else
	mkdir -p $@
endif

$(C_OBJS): $(BUILD_DIR)/%.o : %.c | $(BUILD_DIRS)
	$(CC) $(CFLAGS) $(CPPFLAGS) $< -MMD -MF $(@:.o=.d) -c -o $@

clean:
ifeq ($(OS),Windows_NT)
	if exist $(BUILD_DIR) rmdir /S /Q $(BUILD_DIR)
else
	rm -rf $(BUILD_DIR)
endif

-include $(C_DEPS)

.PHONY: all nrm clean
