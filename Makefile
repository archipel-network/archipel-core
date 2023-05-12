###############################################################################
# Default Commands
###############################################################################

.PHONY: all
all: posix

.PHONY: ud3tn
ud3tn: posix

.PHONY: clean
clean::
	$(RM) -rf build/

###############################################################################
# Execution and Deployment
###############################################################################

.PHONY: run-posix
run-posix: posix
	build/posix/ud3tn

.PHONY: run-unittest-posix
run-unittest-posix: unittest-posix
	build/posix/testud3tn


###############################################################################
# Tools
###############################################################################

.PHONY: gdb-posix
gdb-posix: posix
	$(TOOLCHAIN_POSIX)gdb build/posix/ud3tn


###############################################################################
# Tests
###############################################################################

.PHONY: integration-test
integration-test:
	pytest test/integration

.PHONY: integration-test-tcpspp
integration-test-tcpspp:
	CLA=tcpspp pytest test/integration

.PHONY: integration-test-tcpcl
integration-test-tcpcl:
	CLA=tcpcl pytest test/integration

.PHONY: integration-test-mtcp
integration-test-mtcp:
	CLA=mtcp pytest test/integration


# Directory for the virtual Python envionment
VENV := .venv

ifeq "$(verbose)" "yes"
  PIP = pip
else
  PIP = pip -q
  GET_PIP += > /dev/null
endif

.PHONY: virtualenv
virtualenv:
	@echo "Create virtualenv in $(VENV)/ ..."
	@python3 -m venv $(VENV)
	@echo "Install/update dependencies..."
	. $(VENV)/bin/activate && $(MAKE) update-virtualenv
	@echo
	@echo "=> To activate the virtualenv, source $(VENV)/bin/activate"
	@echo "   or use environment-setup tools like"
	@echo "     - virtualenv"
	@echo "     - virtualenvwrapper"
	@echo "     - direnv"

.PHONY: update-virtualenv
update-virtualenv:
	@echo "Update setuptools, pip, wheel..."
	$(PIP) install -U setuptools pip wheel
	@echo "Install local dependencies to site-packages..."
	$(PIP) install -e ./pyd3tn
	$(PIP) install -e ./python-ud3tn-utils
	@echo "Install additional dependencies ..."
	$(PIP) install -U -r ./test/integration/requirements.txt
	$(PIP) install -U -r ./tools/analysis/requirements.txt

###############################################################################
# Code Quality Tests (and Release Tool)
###############################################################################

.PHONY: check-style
check-style:
	bash ./tools/analysis/stylecheck.sh

.PHONY: clang-check-posix
clang-check-posix: ccmds-posix
	bash ./tools/analysis/clang-check.sh clang-check \
		"posix" \
		"components/agents/posix" \
		"components/cla/posix" \
		"components/platform/posix"

.PHONY: clang-tidy-posix
clang-tidy-posix: ccmds-posix
	bash ./tools/analysis/clang-check.sh "clang-tidy --use-color" \
		"posix" \
		"components/agents/posix" \
		"components/cla/posix" \
		"components/platform/posix"

###############################################################################
# Flags
###############################################################################

CPPFLAGS += -Wall

ifeq "$(type)" "release"
  CPPFLAGS += -O2
else
  CPPFLAGS += -g -O0 -DDEBUG
endif

ifneq "$(wextra)" "no"
  ifeq "$(wextra)" "all"
    CPPFLAGS += -Wextra -Wconversion -Wundef -Wshadow -Wsign-conversion -Wformat-security
  else
    CPPFLAGS += -Wextra -Wno-error=extra -Wno-unused-parameter
    ifneq ($(TOOLCHAIN),clang)
      CPPFLAGS += -Wno-override-init -Wno-unused-but-set-parameter
    endif
  endif
endif

ifeq "$(werror)" "yes"
  CPPFLAGS += -Werror
endif

ifneq "$(verbose)" "yes"
  Q := @
  quiet := quiet_
  MAKEFLAGS += --no-print-directory
endif

ifeq "$(sanitize-strict)" "yes"
  sanitize ?= yes
  ARCH_FLAGS += -fno-sanitize-recover=address,undefined
  ifeq "$(TOOLCHAIN)" "clang"
    ARCH_FLAGS += -fno-sanitize-recover=unsigned-integer-overflow,implicit-conversion,local-bounds
  endif
endif

ifeq "$(sanitize)" "yes"
  ARCH_FLAGS += -fsanitize=address -fno-omit-frame-pointer -fsanitize=undefined
  ifeq "$(TOOLCHAIN)" "clang"
    ARCH_FLAGS += -fsanitize=unsigned-integer-overflow,implicit-conversion,local-bounds
  endif
else
  ifeq "$(TOOLCHAIN)" "clang"
    ifeq "$(sanitize)" "memory"
      ARCH_FLAGS += -fsanitize=memory -fsanitize-memory-track-origins
      ifeq "$(sanitize-strict)" "yes"
        ARCH_FLAGS += -fno-sanitize-recover=memory
      endif
    endif
    ifeq "$(sanitize)" "thread"
      ARCH_FLAGS += -fsanitize=thread
      ifeq "$(sanitize-strict)" "yes"
        ARCH_FLAGS += -fno-sanitize-recover=thread
      endif
    endif
  endif
endif

-include config.mk

###############################################################################
# uD3TN-Builds
###############################################################################

.PHONY: posix

ifndef PLATFORM

posix:
	@$(MAKE) PLATFORM=posix posix

posix-lib:
	@$(MAKE) PLATFORM=posix posix-lib

unittest-posix:
	@$(MAKE) PLATFORM=posix unittest-posix

ccmds-posix:
	@$(MAKE) PLATFORM=posix build/posix/compile_commands.json

else # ifndef PLATFORM

include mk/$(PLATFORM).mk
include mk/build.mk

posix: build/posix/ud3tn
posix-lib: build/posix/libud3tn.so
unittest-posix: build/posix/testud3tn

endif # ifndef PLATFORM
