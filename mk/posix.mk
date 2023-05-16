# BASIC TOOLCHAIN CONFIG

TOOLCHAIN_POSIX ?=
GCC_TOOLCHAIN_PREFIX ?= $(TOOLCHAIN_POSIX)
CLANG_PREFIX ?=
CLANG_SYSROOT_POSIX ?=
CLANG_SYSROOT ?= $(CLANG_SYSROOT_POSIX)
CPU ?=

# COMPILER AND LINKER FLAGS

CPPFLAGS += -DPLATFORM_POSIX -pipe -fPIC

ifdef ARCH
  CPPFLAGS += -march=$(ARCH)
else
  UNAME_M := $(shell uname -m)
  ifneq ($(strip $(filter %86 %86_64,$(UNAME_M))),)
    CPPFLAGS += -march=native
  endif
endif

LDFLAGS += -lpthread
LDFLAGS_EXECUTABLE += -pie
LDFLAGS_LIB += -shared

ifeq "$(type)" "release"
  CPPFLAGS += -ffunction-sections -fdata-sections \
              -D_FORTIFY_SOURCE=2 -fstack-protector-strong \
              --param ssp-buffer-size=4 -Wstack-protector \
              -fno-plt
  LDFLAGS += -flto -Wl,-z,relro,-z,now,-z,noexecstack
  LDFLAGS_EXECUTABLE += -Wl,--gc-sections,--sort-common,--as-needed
endif
