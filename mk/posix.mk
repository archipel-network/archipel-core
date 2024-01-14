# BASIC TOOLCHAIN CONFIG

TOOLCHAIN_POSIX ?=
GCC_TOOLCHAIN_PREFIX ?= $(TOOLCHAIN_POSIX)
CLANG_PREFIX ?=
CLANG_SYSROOT_POSIX ?=
CLANG_SYSROOT ?= $(CLANG_SYSROOT_POSIX)

# COMPILER AND LINKER FLAGS

CPPFLAGS += -DPLATFORM_POSIX -pipe -fPIC

# Provide POSIX plus Linux and BSD symbols and extensions.
# https://www.gnu.org/software/libc/manual/html_node/Feature-Test-Macros.html
CPPFLAGS += -D_XOPEN_SOURCE=700 -D_DEFAULT_SOURCE -DARCHIPEL_CORE

ifdef ARCH
  CPPFLAGS += -march=$(ARCH)
else
  UNAME_M := $(shell uname -m)
  ifneq ($(strip $(filter %86 %86_64,$(UNAME_M))),)
    CPPFLAGS += -march=native
  endif
endif

# OS DETECTION

UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
  EXPECT_MACOS_LINKER ?= 1
else
  EXPECT_MACOS_LINKER ?= 0
endif

# PLATFORM-SPECIFIC FLAGS

LDFLAGS += -lpthread
LDFLAGS_LIB += -shared

ifneq ($(EXPECT_MACOS_LINKER),1)
  LDFLAGS_EXECUTABLE += -pie
endif

ifeq "$(type)" "release"
  CPPFLAGS += -ffunction-sections -fdata-sections \
              -D_FORTIFY_SOURCE=2 -fstack-protector-strong \
              --param ssp-buffer-size=4 -Wstack-protector \
              -fno-plt
  LDFLAGS += -flto
  ifneq ($(EXPECT_MACOS_LINKER),1)
    LDFLAGS += -Wl,-z,relro,-z,now,-z,noexecstack
    LDFLAGS_EXECUTABLE += -Wl,--gc-sections,--sort-common,--as-needed
  endif
endif
