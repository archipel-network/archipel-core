# TOOLCHAIN

TOOLCHAIN ?= gcc

ifeq ($(TOOLCHAIN),clang)
  CLANG  ?= $(CLANG_PREFIX)clang
  CC     := $(CLANG)
  CXX    := $(CLANG)
else
  CC     := $(GCC_TOOLCHAIN_PREFIX)gcc
  CXX    := $(GCC_TOOLCHAIN_PREFIX)g++
endif

AS       := $(GCC_TOOLCHAIN_PREFIX)gcc
LD       := $(GCC_TOOLCHAIN_PREFIX)gcc
AR       := $(GCC_TOOLCHAIN_PREFIX)ar
RANLIB   := $(GCC_TOOLCHAIN_PREFIX)ranlib
OBJCOPY  := $(GCC_TOOLCHAIN_PREFIX)objdump
OBJCOPY  := $(GCC_TOOLCHAIN_PREFIX)objcopy

# COMMON FLAGS

CPPFLAGS += $(ARCH_FLAGS) -MMD -Iinclude $(EXTERNAL_INCLUDES)

CFLAGS += -std=gnu99
CXXFLAGS += -fno-exceptions -fno-rtti

ASFLAGS += $(ARCH_FLAGS)

LDFLAGS += $(ARCH_FLAGS) -lc -lm

ifeq ($(TOOLCHAIN),clang)
  ifneq ($(CLANG_SYSROOT),)
    CPPFLAGS += --sysroot $(CLANG_SYSROOT)
  endif
endif

# COMMANDS

echo-cmd = $(if $($(quiet)cmd_$1),echo '$($(quiet)cmd_$1)';)
cmd      = @$(call echo-cmd,$1,$2) $(cmd_$1)

quiet_cmd_cc_o_c   = CC      $@
cmd_cc_o_c         = "$(CC)" -o "$@" $(CFLAGS) $(CPPFLAGS) -c "$<"

quiet_cmd_cc_o_cxx = CXX     $@
cmd_cc_o_cxx       = "$(CXX)" -o "$@" $(CXXFLAGS) $(CPPFLAGS) -c "$<"

quiet_cmd_cc_o_s   = AS      $@
cmd_cc_o_s         = "$(AS)" -o "$@" $(ASFLAGS) -c "$<"

quiet_cmd_ar       = AR      $@
cmd_ar             = "$(AR)" rcs "$@" $(OBJECTS)

quiet_cmd_link     = LINK    $@
cmd_link           = "$(LD)" -o "$@" "-Wl,-Map=$@.map" \
                     -Wl,--start-group $(OBJECTS) $(LIBS) $(LDFLAGS) -Wl,--end-group

quiet_cmd_mkdir    = MKDIR   $@
cmd_mkdir          = mkdir -p "$@"

quiet_cmd_rm       = RM      $2
cmd_rm             = rm -rf "$2"

quiet_cmd_bin      = BIN     $@
cmd_bin            = "$(OBJCOPY)" -O binary "$<" "$@"

quiet_cmd_ihex     = IHEX    $@
cmd_ihex           = "$(OBJCOPY)" -O ihex "$<" "$@"

quiet_cmd_ccmd_o_x = CCMD    $@
cmd_ccmd_o_x       = echo '{"command": "cc -o \"$(patsubst %.ccmd,%.o,$@)\" \
                     $(CFLAGS) $(CPPFLAGS) -c \"$<\"", "directory": "$(CURDIR)", \
                     "file": "$<"}' > "$@"

quiet_cmd_ccmds    = CCMDS   $@
cmd_ccmds          = echo "[$$(sed -s -e '$$a,' $(CCMDS) | sed -e '$$d')]" > "$@"

# RULE GENERATION

define generateComponentRules

component_C_SOURCES := $(if $(2),$(patsubst %.c,$(1)/%.c,$(filter %.c,$(2))),$$(wildcard $(1)/*.c))
component_CXX_SOURCES := $(if $(2),$(patsubst %.cpp,$(1)/%.cpp,$(filter %.cpp,$(2))),$$(wildcard $(1)/*.cpp))
component_ASM_SOURCES := $(if $(2),$(patsubst %.s,$(1)/%.s,$(filter %.s,$(2))),$$(wildcard $(1)/*.s))

component_OBJECTS := $$(patsubst $(1)/%.c,build/$(PLATFORM)/$(1)/%.o,$$(component_C_SOURCES)) \
                     $$(patsubst $(1)/%.cpp,build/$(PLATFORM)/$(1)/%.o,$$(component_CXX_SOURCES)) \
                     $$(patsubst $(1)/%.s,build/$(PLATFORM)/$(1)/%.o,$$(component_ASM_SOURCES)) \

component_DEPS := $$(patsubst $(1)/%.c,build/$(PLATFORM)/$(1)/%.d,$$(component_C_SOURCES)) \
                  $$(patsubst $(1)/%.cpp,build/$(PLATFORM)/$(1)/%.d,$$(component_CXX_SOURCES))
-include $$(component_DEPS)

build/$(PLATFORM)/$(1)/%.o: $(1)/%.c | build/$(PLATFORM)/$(1)
	$$(call cmd,cc_o_c)

build/$(PLATFORM)/$(1)/%.o: $(1)/%.cpp | build/$(PLATFORM)/$(1)
	$$(call cmd,cc_o_cxx)

build/$(PLATFORM)/$(1)/%.o: $(1)/%.s | build/$(PLATFORM)/$(1)
	$$(call cmd,cc_o_s)

build/$(PLATFORM)/$(1)/component.a: OBJECTS := $$(component_OBJECTS)
build/$(PLATFORM)/$(1)/component.a: $$(component_OBJECTS)
build/$(PLATFORM)/$(1)/component.a: | build/$(PLATFORM)/$(1)
	$$(call cmd,ar)

build/$(PLATFORM)/$(1): | build/$(PLATFORM)
	$$(call cmd,mkdir)

# Clang compile_commands.json

component_CCMDS := $$(patsubst %.o,%.ccmd,$$(component_OBJECTS))

build/$(PLATFORM)/$(1)/%.ccmd: $(1)/%.c | build/$(PLATFORM)/$(1)
	$$(call cmd,ccmd_o_x)

build/$(PLATFORM)/$(1)/%.ccmd: $(1)/%.cpp | build/$(PLATFORM)/$(1)
	$$(call cmd,ccmd_o_x)

build/$(PLATFORM)/$(1)/%.ccmd: $(1)/%.s | build/$(PLATFORM)/$(1)
	$$(call cmd,ccmd_o_x)

build/$(PLATFORM)/compile_commands.json: CCMDS := $$(component_CCMDS)
build/$(PLATFORM)/compile_commands.json: $$(component_CCMDS)
build/$(PLATFORM)/compile_commands.json: | build/$(PLATFORM)

endef

define addComponent

LIBS_$(1) += build/$(PLATFORM)/$(2)/component.a

build/$(PLATFORM)/$(1): build/$(PLATFORM)/$(2)/component.a
build/$(PLATFORM)/$(1): | build/$(PLATFORM)

endef

define addComponentWithRules

$(call generateComponentRules,$(1),$(2))

$(call addComponent,libud3tn.so,$(1))
$(call addComponent,ud3tn,$(1))
$(call addComponent,testud3tn,$(1))

endef

build/$(PLATFORM)/compile_commands.json:
build/$(PLATFORM)/compile_commands.json: | build/$(PLATFORM)
	$(call cmd,ccmds)
