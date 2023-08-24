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
# Install
###############################################################################

.PHONY: install-posix
install-posix: posix
	-useradd -r archipel
	mkdir -p /var/run/archipel-core
	chown archipel:archipel /var/run/archipel-core
	chmod 775 /var/run/archipel-core
	mkdir -p /usr/share/archipel-core
	mkdir -p /etc/archipel-core
	rm -f /usr/share/archipel-core/archipel-core
	rm -f /etc/systemd/system/archipel-core.service
	rm -f /etc/systemd/user/archipel-core.service
	cp -f build/posix/ud3tn /usr/share/archipel-core/
	cp -f archipel-core.service /etc/systemd/system/
	cp -f user-archipel-core.service /etc/systemd/user/archipel-core.service
	ln -f -s /usr/share/archipel-core/archipel-core /usr/bin/archipel-core
	cd pyd3tn && python3 setup.py install
	cd python-ud3tn-utils && python3 setup.py install

.PHONY: install-linked-posix
install-linked-posix: posix
	-useradd -r archipel
	mkdir -p /var/run/archipel-core
	chown archipel:archipel /var/run/archipel-core
	chmod 775 /var/run/archipel-core
	mkdir -p /etc/archipel-core
	rm -f /usr/share/archipel-core/archipel-core
	rm -f /etc/systemd/system/archipel-core.service
	rm -f /etc/systemd/user/archipel-core.service/archipel-core.service
	ln -f -s "$(shell pwd)/archipel-core.service" /etc/systemd/system/archipel-core.service
	ln -f -s "$(shell pwd)/user-archipel-core.service" /etc/systemd/user/archipel-core.service
	ln -f -s "$(shell pwd)/build/posix/ud3tn" /usr/bin/archipel-core
	cd pyd3tn && python3 setup.py install
	cd python-ud3tn-utils && python3 setup.py install

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
	@$(PIP) install -e ./pyd3tn
	@$(PIP) install -e ./python-ud3tn-utils
	@echo "Install additional dependencies ..."
	@$(PIP) install -U -r ./test/integration/requirements.txt
	@$(PIP) install -U -r ./tools/analysis/requirements.txt
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

###############################################################################
# Code Quality Tests (and Release Tool)
###############################################################################

.PHONY: check-style
check-style:
	bash ./tools/analysis/stylecheck.sh

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
