# BASIC TOOLCHAIN CONFIG

TOOLCHAIN_POSIX ?=
GCC_TOOLCHAIN_PREFIX ?= $(TOOLCHAIN_POSIX)
CLANG_PREFIX ?=
CLANG_SYSROOT_POSIX ?=
CLANG_SYSROOT ?= $(CLANG_SYSROOT_POSIX)
CPU ?=

# COMPILER AND LINKER FLAGS

CPPFLAGS += -DPLATFORM_POSIX -pipe -fPIE

ifdef ARCH
  CPPFLAGS += -march=$(ARCH)
else
  CPPFLAGS += -march=native
endif

LDFLAGS += -lpthread
LDFLAGS_EXECUTABLE += -pie
LDFLAGS_LIB += -shared

ifeq "$(type)" "release"
  CPPFLAGS += -ffunction-sections -fdata-sections \
              -D_FORTIFY_SOURCE=2 -fstack-protector-strong \
              --param ssp-buffer-size=4 -Wstack-protector \
              -fno-plt
  LDFLAGS += -Wl,--gc-sections,--sort-common,--as-needed -flto \
             -Wl,-z,relro,-z,now,-z,noexecstack
endif
