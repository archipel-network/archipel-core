# µD3TN Testing Documentation

µD3TN relies on intensive testing to ensure system stability as well as project quality. The following information should be taken into account by all project contributors.

There are currently three ways to check for code quality issues and verify the proper function of µD3TN:
* Static analysis
* Unit tests
* Integration tests

The tests can be either executed manually, or automatically via a Continuous Integration server. This document summarizes which tests are available and how to execute them.

For patches or pull requests to be accepted, _all_ of the below-listed tests have to return a successful result on _all_ platforms.

## Static Analysis

### Compiler Warnings

Most of the available compiler warnings are turned on automatically in the Makefile. This usually gives already an indication of some obvious problems in new code. To check that there are no problems compiling µD3TN for various platforms, the following commands should be run:

```
make clean && make posix type=debug werror=yes
make clean && make posix type=release werror=yes
make clean && make posix TOOLCHAIN=clang werror=yes
make clean && make stm32 werror=yes
```

### Linter (clang-tidy)

Clang Tidy is an extensible linter that can be used to check for some typical programming errors. It can be executed for µD3TN as follows:

```
make clang-tidy-posix
```

### Stylecheck

The `checkpatch.pl` utility available in the Linux kernel source tree is used for µD3TN to enforce compatibility with the project's coding style which is the one used for the Linux kernel.
The coding style compliance check can be executed by running:

```
make check-style
```

No errors or warnings must be shown.

## Unit Tests

For unit testing, the lightweight [Unity](http://www.throwtheswitch.org/unity/) test framework is used. It provides a simple API and everything necessary to check assertions and generate a test report. Most test cases are available and run for the POSIX as well as the STM32 platform.

The tests are located in `test/unit` and, including the Unity test framework, are compiled into the µD3TN test binary (via `make unittest-posix`).

### POSIX

To run all unit tests against the POSIX port of µD3TN, a `make` command is provided:

```
make run-unittest-posix
```

This will automatically build µD3TN plus the tests and execute them. An output similar to the following should be displayed:

```
[...]
......................................

-----------------------
44 Tests 0 Failures 0 Ignored
OK
[Mon Mar 26 14:26:00 2018]: Unittests finished without errors! SUCCESS! (-1) [components/test/src/main.c:72]
```

### STM32

First, one needs to configure building for the STM32 platform via `config.mk`, as discussed in the [README](../README.md). Now, tests can be built and flashed to the STM32 board via:

```
make flash-unittest-stm32-openocd-oneshot
```

This uses the `sopenocd` utility to upload the tests to the board. They are executed automatically on every boot. The output is provided via the USB Virtual COM Port and should be similar to the one on POSIX, as mentioned above.

## Integration Tests

There are several integration test scenarios which check µD3TN's behavior. For the integration tests to work, an instance of µD3TN first has to be started and the Python `venv` has to be activates. For the latter, see [python-venv.md](python-venv.md).

**Example for POSIX:**

Start µD3TN in a dedicated terminal:

```
make run-posix
```

Start the integration tests in a separate terminal:

```
source .venv/bin/activate
make integration-test
```

**Example for STM32:**

Flash µD3TN to the board, preferably as a `release` build to prevent delays due to debug output.

```
make clean
make flash-stm32-openocd-oneshot
```

If `openocd` is already running, you can use the command `make flash-stm32-openocd` instead.

As the integration test framework expects a TCP socket (running the MTCPCL protocol), invoke the helper script in another terminal to provide this socket:

```
source .venv/bin/activate
make connect
```

Note: A little less convenient option would be to use `socat`, e.g., via: `socat /dev/ttyACM0 tcp-listen:4222`

Now, the tests can be run.

```
source .venv/bin/activate
make integration-test-stm32
```

**STM32 Debugging:**

If you encounter any issues with the tests or while developing new features for STM32, you can attach a debugger as follows:

* Start `openocd` in one terminal and
* Invoke `make gdb-stm32` in another terminal with the same options you used for compiling `ud3tn`.

The debugger should automatically connect to `openocd` and stop the currently-run program. You may want to issue a `continue` command, now. Otherwise, you can work with the GDB instance as usual, i.e., set breakpoints, single-step through the code, print backtraces, and so forth.

If the program crashes and you have compiled with `type=debug` (which is the default), you may find yourself with output similar to the following:

```
Program received signal SIGTRAP, Trace/breakpoint trap.
halt_fault (fault=0x8028d64 "BusFault", task=0x8028d40 "< NO TASK >", r0=268445368, r1=2779096485, r2=6, r3=2779096485, r12=0, lr=0x80059eb <bundle_is_equal+18>, pc=0x8005a2e <bundle_is_equal_parent+16>,
    psr=2701197312, scb_shcsr=458754, cfsr=33280, hfsr=0, dfsr=11, afsr=0, bfar=2779096485) at components/platform/stm32/fault_handler.c:26
26			asm volatile ("bkpt");
```

This means that the hardware encountered a fault (e.g., an invalid memory access) and cannot continue operation. At this point, µD3TN's own fault handler has been invoked with the current contents of all important registers. You can now inspect the memory or simply set a breakpoint at the last position of the program counter (the `pc` argument of this function), which should be the point at which the program crashes:

```
(gdb) break *0x8005a2e
(gdb) continue
<Press RESET on STM32 board>
```
