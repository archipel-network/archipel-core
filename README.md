µPCN - A Free and Lean DTN Implementation for Microcontrollers and POSIX
=======================================================================

See `./LICENSE.txt` and `./LICENSE-3RD-PARTY.txt` for legal information.

**Note:** Currently the documentation does not really deserve its name.
Additionally, the command line options do not follow the POSIX guidelines so far.
A basic Manpage can be found in the doc folder (upcn.1).

Platforms
---------

µPCN currently supports two platforms:
1. The STM32F4 embedded platform running FreeRTOS
2. All POSIX-compliant operating systems (plus Linux ;-))

Quick Start
-----------

Both platforms can be built, deployed, tested and used in parallel.
To get started with one or both platforms, just follow the subsequent
instructions.

#### Quick Start for STM32F4

For this platform only the STM32F4 embedded system is supported currently.
However, porting to other Cortex-M3/M4 based SoCs should be trivial.

In order to run µPCN on bare metal (e.g. the STM32F4Discovery board)
three steps are necessary after connecting the board via STLink-enabled USB:

1. Install or unpack the following dependencies:
   - The `gcc-arm-none-eabi` toolchain including `newlib`.
   - The [Open On-Chip Debugger](http://openocd.org/).
   - A version of [FreeRTOS](https://www.freertos.org/) (uPCN is currently tested with version 9).
   - For debugging and testing you may want to install GDB and a recent version
     of Python 3 (>= 3.6).

2. Copy `config.mk.example` to `config.mk` and set at least the following
   variables according to your installation:
   - Set `TOOLCHAIN_STM32` to the prefix for your *arm-none-eabi* toolchain.
     If installed with the distribution's package manager, this is commonly
     `/usr/bin/arm-none-eabi-`.
   - Set `FREERTOS_PATH` to the path to your FreeRTOS source.

3. Start `openocd`. The config file `openocd.cfg` is loaded automatically.

4. Type `make flash-stm32-openocd` to build the project and to flash `upcn.bin`
   to the board attached via USB.
   The unittests can be flashed via `make flash-unittest-stm32-openocd`.
   For convenience, GDB commands are available via `make gdb-stm32` and
   `make gdb-unittest-stm32`.

#### Quick Start for POSIX-compliant Operating Systems

1. Install or unpack the following dependencies:
   - The `gcc` toolchain and `binutils` for your local system.
   - For building with Clang, install a recent version of Clang and LLVM.
   - For debugging and testing you may want to install GDB and a recent version
     of Python 3 (>= 3.6).

2. If the build tools are not located in `/usr/bin` for your system, you may
   copy `config.mk.example` to `config.mk` and set the
   `GCC_TOOLCHAIN_PREFIX_POSIX` variable to the prefix for your
   local `gcc` toolchain (default: `/usr/bin`).

3. Type `make run-posix` to build and execute µPCN on your local machine.

Getting Started with the Implementation
---------------------------------------

The core part of uPCN is located in `./components/upcn/`.
The starting point of the program can be found in
`./components/daemon/main.c`, calling init located in
`./components/upcn/init.c`.
This file is the best place to familiarize with the implementation.

Configurability
---------------

The following values can be configured at runtime via bundles to the "/config"
endpoint (see `doc/contacts_data_format.md`):
- reachable nodes (EID and CLA address)
- contacts (interval and data rate)
- reachable EIDs for each node and contact
- reliability and trustworthiness of the nodes

The following values can be configured at runtime via bundles to the
"/management" endpoint:
- system time

Via command line parameters (see `-h` or `--help` for details):
- own EID
- used CLAs and their parameters (e.g. IP and port of TCP socket)
- BP version for bundles injected by applications (6: RFC5050, 7: BPbis)
- Application Agent: IP and port
- maximum bundle size
- lifetime of bundles injected by applications
- activation of status reports

Via `config.h` at compile time:
- default values for command line parameters
- available storage space for bundles
- limits of the custody transfer feature
- settings of TCP connections created by various CLAs (e.g. timeouts, retry intervals)
- tuning of the routing algorithm regarding reliability and trustworthiness of nodes
- thread priorities, queue lengths, further performance settings (e.g. size of hash tables)

For configuring contacts at runtime, the `aap_config.py` script in `tools/aap`
may be used.

Testing
-------

Details about tools for testing and the overall testing approaches can be found
in `./doc/testing.md`.

License
-------

The code in `./components`, `./include`, `./pyupcn`, `./test`, and `./tools`
has been developed specifically for µPCN and is released under a BSD 3-clause
license. The license can be found in `./LICENSE.txt`.

External code
-------------

As an early starting point for the STM32F4 project structure,
we have used the project https://github.com/elliottt/stm32f4/
as a general basis.

All further code taken from third parties is documented in
`./LICENSE-3RD-PARTY.txt`, as well as in the source files, along with the
respective original URLs and associated licenses. Generally, third-party code
is found in `./external` and uses Git submodules referencing the original
repositories where applicable.
