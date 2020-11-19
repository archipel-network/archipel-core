# BASIC TOOLCHAIN CONFIG

TOOLCHAIN_STM32 ?= /usr/bin/arm-none-eabi-
GCC_TOOLCHAIN_PREFIX ?= $(TOOLCHAIN_STM32)
CLANG_PREFIX ?= /usr/bin/
CLANG_SYSROOT_STM32 ?=
CLANG_SYSROOT ?= $(CLANG_SYSROOT_STM32)

FREERTOS_PREFIX ?= $(HOME)/.local/FreeRTOSv9.0.0/

# COMPILER AND LINKER FLAGS

ARCH_FLAGS += -mcpu=cortex-m4 -mlittle-endian -mthumb -march=armv7e-m

CPPFLAGS += -DPLATFORM_STM32 -fshort-enums 

LDSCRIPT_STM32 ?= external/platform/stm32/support/stm32_flash.ld

LDFLAGS  += -Wl,--script=$(LDSCRIPT_STM32)

ifeq ($(TOOLCHAIN),clang)
  # NOTE: It seems the option -fno-integrated-as is not applied by Clang.
  #       Therefore, compilation of the CMSIS library is not possible with HW
  #       floating point support. Therefore, we compile with softfp.
  # CPPFLAGS += -target armv7em-none-eabihf -fno-integrated-as
  CPPFLAGS += -target armv7em-none-eabi
  ARCH_FLAGS += -mfloat-abi=soft
else
  # See above. HW float is currently only supported using GCC.
  ARCH_FLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16
endif

# Even if we run a debug build, cleaning up sections is a good thing.
CPPFLAGS += -ffunction-sections -fdata-sections
LDFLAGS += -Wl,--gc-sections

ifeq "$(type)" "release"
  LDFLAGS += -Wl,--sort-common,--as-needed -flto
endif

# BOARD CONFIG for STM32F4 Discovery

BOARD_FREQ_STM32 ?= 8000000

CPPFLAGS += -DHSE_VALUE=$(BOARD_FREQ_STM32) -DBOARD_STM32F4_DISCOVERY

# BIN FILE GENERATION RULES (for flashing EEPROM)

build/stm32/ud3tn.bin: build/stm32/ud3tn
	$(call cmd,bin)

build/stm32/testud3tn.bin: build/stm32/testud3tn
	$(call cmd,bin)
