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

.PHONY: flash-stm32-stlink
flash-stm32-stlink: stm32
	$(ST_FLASH_PREFIX)st-flash --reset write build/stm32/ud3tn.bin 0x08000000

.PHONY: flash-unittest-stm32-stlink
flash-unittest-stm32-stlink: unittest-stm32
	$(ST_FLASH_PREFIX)st-flash --reset write build/stm32/testud3tn.bin 0x08000000

.PHONY: flash-stm32-openocd
flash-stm32-openocd: stm32
	echo "reset init;" "flash write_image erase build/stm32/ud3tn.bin 0x08000000;" \
		"reset;" "exit;" | ncat localhost 4444 | tail -n +2 > /dev/null;

.PHONY: flash-unittest-stm32-openocd
flash-unittest-stm32-openocd: unittest-stm32
	echo "reset init;" "flash write_image erase build/stm32/testud3tn.bin 0x08000000;" \
		"reset;" "exit;" | ncat localhost 4444 | tail -n +2 > /dev/null;

.PHONY: flash-stm32-openocd-oneshot
flash-stm32-openocd-oneshot: stm32
	$(OOCD_PREFIX)openocd -c "script openocd.cfg" -c "reset init" \
			      -c "flash write_image erase build/stm32/ud3tn.bin 0x08000000" \
			      -c "reset" -c "shutdown"

.PHONY: flash-unittest-stm32-openocd-oneshot
flash-unittest-stm32-openocd-oneshot: unittest-stm32
	$(OOCD_PREFIX)openocd -c "script openocd.cfg" -c "reset init" \
			      -c "flash write_image erase build/stm32/testud3tn.bin 0x08000000" \
			      -c "reset" -c "shutdown"

###############################################################################
# Tools
###############################################################################

.PHONY: gdb-posix
gdb-posix: posix
	$(TOOLCHAIN_POSIX)gdb build/posix/ud3tn

.PHONY: gdb-stm32
gdb-stm32: stm32
	$(TOOLCHAIN_STM32)gdb --eval-command="target remote :4443" \
		build/stm32/ud3tn

.PHONY: gdb-unittest-stm32
gdb-unittest-stm32: unittest-stm32
	$(TOOLCHAIN_STM32)gdb --eval-command="target remote :4443" \
		build/stm32/testud3tn

.PHONY: openocd
openocd:
	openocd

.PHONY: stutil
stutil:
	$$(dirname $(ST_FLASH))/st-util -p 4443

.PHONY: connect
connect:
	python3 ./tools/cla/stm32_mtcp_proxy.py \
		--device /dev/serial/by-id/*STM32_Virtual*

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

.PHONY: integration-test-stm32
integration-test-stm32:
	CLA=usbotg pytest test/integration


# Directory for the virtual Python envionment
VENV := .venv
GET_PIP = curl -sS https://bootstrap.pypa.io/get-pip.py | $(VENV)/bin/python

ifeq "$(verbose)" "yes"
  PIP = $(VENV)/bin/pip
else
  PIP = $(VENV)/bin/pip -q
  GET_PIP += > /dev/null
endif

.PHONY: virtualenv
virtualenv:
	@echo "Create virtualenv in $(VENV)/ ..."
	@python3 -m venv --without-pip $(VENV)
	@echo "Install latest pip package ..."
	@$(GET_PIP)
	@echo "Install local dependencies to site-packages..."
	@$(PIP) install -U -e ./pyd3tn ./python-ud3tn-utils
	@echo "Install additional dependencies ..."
	@$(PIP) install -U -r ./test/integration/requirements.txt
	@$(PIP) install -U -r ./tools/analysis/requirements.txt
	@$(PIP) install -U -r ./tools/cla/requirements.txt
	@echo
	@echo "=> To activate the virtualenv, source $(VENV)/bin/activate"
	@echo "   or use environment-setup tools like"
	@echo "     - virtualenv"
	@echo "     - virtualenvwrapper"
	@echo "     - direnv"

.PHONY: update-virtualenv
update-virtualenv:
	$(PIP) install -U setuptools pip wheel
	$(PIP) install -U -r ./test/integration/requirements.txt
	$(PIP) install -U -r ./tools/analysis/requirements.txt
	$(PIP) install -U -r ./tools/cla/requirements.txt

###############################################################################
# Code Quality Tests (and Release Tool)
###############################################################################

.PHONY: check-style
check-style:
	bash ./tools/analysis/stylecheck.sh

.PHONY: clang-check-stm32
clang-check-stm32: ccmds-stm32
	bash ./tools/analysis/clang-check.sh check \
		"stm32" \
		"components/agents/stm32" \
		"components/cla/stm32" \
		"components/platform/stm32"

.PHONY: clang-tidy-stm32
clang-tidy-stm32: ccmds-stm32
	bash ./tools/analysis/clang-check.sh tidy \
		"stm32" \
		"components/agents/stm32" \
		"components/cla/stm32" \
		"components/platform/stm32"

.PHONY: clang-check-posix
clang-check-posix: ccmds-posix
	bash ./tools/analysis/clang-check.sh check \
		"posix" \
		"components/agents/posix" \
		"components/cla/posix" \
		"components/platform/posix"

.PHONY: clang-tidy-posix
clang-tidy-posix: ccmds-posix
	bash ./tools/analysis/clang-check.sh tidy \
		"posix" \
		"components/agents/posix" \
		"components/cla/posix" \
		"components/platform/posix"

###############################################################################
# Flags
###############################################################################

CPPFLAGS += -Wall

ifdef REMOTE_CONFIGURATION
 CPPFLAGS += -DREMOTE_CONFIGURATION
endif

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

-include config.mk

###############################################################################
# uD3TN-Builds
###############################################################################

.PHONY: posix stm32

ifndef PLATFORM

posix:
	@$(MAKE) PLATFORM=posix posix

posix-lib:
	@$(MAKE) PLATFORM=posix posix-lib

unittest-posix:
	@$(MAKE) PLATFORM=posix unittest-posix

stm32:
	@$(MAKE) PLATFORM=stm32 stm32

unittest-stm32:
	@$(MAKE) PLATFORM=stm32 unittest-stm32

ccmds-posix:
	@$(MAKE) PLATFORM=posix build/posix/compile_commands.json

ccmds-stm32:
	@$(MAKE) PLATFORM=stm32 build/stm32/compile_commands.json

else # ifndef PLATFORM

include mk/$(PLATFORM).mk
include mk/build.mk

posix: build/posix/ud3tn
posix-lib: build/posix/libud3tn.so
unittest-posix: build/posix/testud3tn

stm32: build/stm32/ud3tn.bin
unittest-stm32: build/stm32/testud3tn.bin

endif # ifndef PLATFORM
