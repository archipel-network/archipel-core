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
# Packaging
###############################################################################

PKG_OUT?=build/package/debian
PKG_VERSION?=$(shell git describe --tags | sed -e s/^v//)
PKG_ARCH?=$(shell dpkg --print-architecture)

.PHONY: package-debian
package-debian: export type = "Release"
package-debian: posix
	rm -fr $(PKG_OUT)/archipel-core/
# Setup structure
	mkdir -p $(PKG_OUT)/archipel-core/usr/bin
	mkdir -p $(PKG_OUT)/archipel-core/usr/lib/systemd/system
	mkdir -p $(PKG_OUT)/archipel-core/usr/lib/systemd/user
	mkdir -p $(PKG_OUT)/archipel-core/etc/archipel-core
	mkdir -p $(PKG_OUT)/archipel-core/usr/share/doc/archipel-core
	mkdir -p $(PKG_OUT)/archipel-core/DEBIAN
# Copy build artifacts
	-strip --strip-debug --strip-unneeded -o $(PKG_OUT)/archipel-core/usr/bin/archipel-core build/posix/ud3tn
# Copy documentation
	cat "LICENSE" > $(PKG_OUT)/archipel-core/usr/share/doc/archipel-core/copyright
# Copy systemd service files
	cp -f package/debian/archipel-core/archipel-core.service $(PKG_OUT)/archipel-core/usr/lib/systemd/system/
	cp -f package/debian/archipel-core/archipel.slice $(PKG_OUT)/archipel-core/usr/lib/systemd/system/
# Create control file
	cat package/debian/archipel-core/control | \
		VERSION=$(PKG_VERSION) \
		ARCH=$(PKG_ARCH) \
		envsubst > $(PKG_OUT)/archipel-core/DEBIAN/control
	cp -f package/debian/archipel-core/postinst $(PKG_OUT)/archipel-core/DEBIAN/
	cp -f package/debian/archipel-core/postrm $(PKG_OUT)/archipel-core/DEBIAN/
	cp -f package/debian/archipel-core/prerm $(PKG_OUT)/archipel-core/DEBIAN/
# Create conf files
#	cp default-conf.env $(PKG_OUT)/archipel-core/etc/archipel-core/conf.env
#	echo "/etc/archipel-core/conf.env" >> $(PKG_OUT)/archipel-core/DEBIAN/conffiles
# Create changelog
	cat CHANGELOG | gzip -c -9 > $(PKG_OUT)/archipel-core/usr/share/doc/archipel-core/changelog.Debian.gz
# Package build
	cd $(PKG_OUT) && dpkg-deb --root-owner-group --build archipel-core
# Package linting
	-lintian $(PKG_OUT)/archipel-core.deb

.PHONY: package-pyd3tn-debian
package-pyd3tn-debian:
	cd pyd3tn && python3 setup.py --command-packages=stdeb.command sdist_dsc -d ../$(PKG_OUT)/pyd3tn --with-python3=True --no-python2-scripts=True bdist_deb
	cp $(PKG_OUT)/pyd3tn/*.deb $(PKG_OUT)

.PHONY: package-ud3tn-utils-debian
package-ud3tn-utils-debian:
	cd python-ud3tn-utils && python3 setup.py --command-packages=stdeb.command sdist_dsc -d ../$(PKG_OUT)/ud3tn-utils --with-python3=True --no-python2-scripts=True bdist_deb
	cp $(PKG_OUT)/ud3tn-utils/*.deb $(PKG_OUT)

.PHONY: package-ud3tn-utils-debian
package-debian-all: package-debian package-pyd3tn-debian package-ud3tn-utils-debian

###############################################################################
# Execution and Deployment
###############################################################################

.PHONY: run-posix
run-posix: posix
	build/posix/ud3tn

.PHONY: run-unittest-posix
run-unittest-posix: unittest-posix
	build/posix/testud3tn


.PHONY: run-unittest-posix-with-coverage
run-unittest-posix-with-coverage:
	$(MAKE) run-unittest-posix coverage=yes && geninfo build/posix -b . -o ./coverage1.info && genhtml coverage1.info -o build/coverage && echo "Coverage report has been generated in 'file://$$(pwd)/build/coverage/index.html'"


###############################################################################
# Tools
###############################################################################

.PHONY: gdb-posix
gdb-posix: posix
	$(TOOLCHAIN_POSIX)gdb build/posix/ud3tn

.PHONY: aap2-proto-headers
aap2-proto-headers:
	python3 external/nanopb/generator/nanopb_generator.py -Icomponents --output-dir=generated --error-on-unmatched aap2/aap2.proto
	protoc -Icomponents/aap2 --python_out=python-ud3tn-utils/ud3tn_utils/aap2/generated/ aap2.proto

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
	$(PIP) install -U -r ./external/nanopb/extra/requirements.txt

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

-include config.mk

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

ifeq "$(coverage)" "yes"
  ARCH_FLAGS += --coverage
endif

###############################################################################
# uD3TN-Builds
###############################################################################

.PHONY: posix posix-lib posix-all unittest-posix ccmds-posix

ifndef PLATFORM

posix:
	@$(MAKE) PLATFORM=posix posix

posix-lib:
	@$(MAKE) PLATFORM=posix posix-lib

posix-all:
	@$(MAKE) PLATFORM=posix posix posix-lib

unittest-posix:
	@$(MAKE) PLATFORM=posix unittest-posix

data-decoder:
	@$(MAKE) PLATFORM=posix data-decoder

ccmds-posix:
	@$(MAKE) PLATFORM=posix build/posix/compile_commands.json

else # ifndef PLATFORM

include mk/$(PLATFORM).mk
include mk/build.mk

posix: build/posix/ud3tn
posix-lib: build/posix/libud3tn.so build/posix/libud3tn.a
posix-all: posix posix-lib
data-decoder: build/posix/ud3tndecode
unittest-posix: build/posix/testud3tn
ccmds-posix: build/posix/compile_commands.json

endif # ifndef PLATFORM
