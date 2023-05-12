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

For unit testing, the lightweight [Unity](http://www.throwtheswitch.org/unity/) test framework is used. It provides a simple API and everything necessary to check assertions and generate a test report.

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

## Interoperability Tests

µD3TN ships with simple interoperability test scenarios that validate BPv6 and BPv7 interoperability with JPL's ION and are described in the following subsections.
At the end of this section, some hints on doing manual interoperability tests are provided.

### Prerequisites

All tests expect an environment with ION and µD3TN (incl. Python dependencies) installed. There are two options to set up all requirements:

1. Use the Docker image:

    - Run a container built from the [`registry.gitlab.com/d3tn/ud3tn-docker-images/ion-interop:<version>`](https://gitlab.com/d3tn/ud3tn-docker-images/container_registry/3909796) image, whereas `<version>` is the ION version you would like to test against. The µD3TN installation/source should be mounted inside the container and the µD3TN binary should be built with the settings you would like to test.

        ```
        docker run -it -v "<ud3tn-path>:/ud3tn" registry.gitlab.com/d3tn/ud3tn-docker-images/ion-interop:<version> bash
        ```

    - Activate the Python virtual environment and prepare the µD3TN modules:

        ```
        source /ud3tn_venv/bin/activate
        pip install -e "/ud3tn/pyd3tn"
        pip install -e "/ud3tn/python-ud3tn-utils"
        ```

    - You can accelerate these two steps by using the script we maintain for our CI toolchain. Replace `<version>` by the ION version and `<build-args>` by additional arguments to be passed to µD3TN's `make` command (e.g. for enabling sanitizers or other compile-time preferences; may be left out). The script builds µD3TN and sets up the Python dependencies. It assumes that the µD3TN source is mounted under `/ud3tn` and one of our Docker images is used.

        ```
        cd <ud3tn-path>
        docker run -it -v "$(pwd):/ud3tn" registry.gitlab.com/d3tn/ud3tn-docker-images/ion-interop:<version> \
            bash -c '/ud3tn/test/ion_interoperability/prepare_for_test.sh /ud3tn /ud3tn_build <build-args> && cd /ud3tn_build && source /ud3tn_venv/bin/activate && bash'
        ```

    - Now you can `cd /ud3tn` and run the test scripts from there.

2. Set up the environment manually.

    - Install ION according to the instructions provided with it.
    - Obtain the version of µD3TN you would like to test (or mount it as a volume if you are using a container).
    - Install µD3TN's dependencies and build µD3TN as documented in [README.md](../README.md#develop) and the [quick start guide](./posix_quick_start_guide.md#prerequisites)
    - Install the Python dependencies as documented in the [quick start guide](./posix_quick_start_guide.md#python-dependencies)

### Minimal Forwarding Test

This test, provided in [`test/ion_interoperability/minimal_forwarding_test/run.sh`](../test/ion_interoperability/minimal_forwarding_test/run.sh), checks the following aspects:

- forwarding bundles from µD3TN to another instance of µD3TN via an ION instance
- receiving bundles sent by ION (via `bpsource`) in µD3TN (via `aap_receive.py`)
- receiving bundles sent by µD3TN (via `aap_send.py`) in ION (via `bprecvfile`)

The script [`run.sh`](../test/ion_interoperability/minimal_forwarding_test/run.sh) expects two arguments:

```
/path/to/run.sh <bp-version> <scheme>
```

- `<bp-version>`: the Bundle Protocol version to be used, i.e., `6` or `7`
- `<scheme>`: the EID scheme to be used, i.e., `dtn` or `ipn`

When invoked, the script will start ION (via `ionstart -I`) and two µD3TN instances, configure µD3TN to connect to ION via TCPCL, and try to send three bundles to test the three aspects mentioned above.
The ION configuration (`rc`) files can be found in the same directory as `run.sh`, for the `dtn` and `ipn` EID schemes, respectively.
On completion, ION will be shut down using `ionstop` and the µD3TN instances will receive a `SIGTERM`.

### BIBE Interoperability Test

A second test script checks whether forwarding of a bundle encapsulated using [BIBE](https://datatracker.ietf.org/doc/draft-ietf-dtn-bibect/) works via ION, to validate µD3TN's BIBE implementation.

This test always uses BPv7 and the `dtn` scheme, thus, no command line arguments are necessary. The used ION configuration ([`ionstart.rc`](../test/ion_interoperability/bibe_forwarding_test/ionstart.rc)) can be found in the test directory.

### Manual Interoperability Testing

Beside using the prepared test scripts, manual interoperability testing is possible. Developers are advised to check out the "minimal forwarding test" shell script ([`run.sh`](../test/ion_interoperability/minimal_forwarding_test/run.sh)) to get an idea which commands have to be executed.

In summary:

1. Prepare the environment [as described above](#prerequisites).
2. Start ION and configure it as needed:

    - If ION should receive bundles from µD3TN, there should be at least one induct configured that µD3TN can connect to.
    - If ION should send bundles to µD3TN, you should configure a) an outduct and b) an egress plan. Check out the [configuration of the minimal forwarding test](../test/ion_interoperability/minimal_forwarding_test/ipn.rc) to get an idea of this.
    - Note that the only Convergence Layer Adapter currently supported in both µD3TN and ION is TCPCLv3 (µD3TN option `-c tcpclv3:<host>,<port>`).

3. Start µD3TN instances as needed. If you want to test with multiple instances, don't forget to specify different sockets/ports for AAP (via `-s` or `-a`/`-p`) and the CLAs (via `-c`). Refer to the [README.md](../README.md#usage) concerning the command line syntax. You can also find some pointers on running multiple instances in the [quick start guide](./posix_quick_start_guide.md).

4. Configure the µD3TN instance(s) as needed:
   Especially, add all outgoing contacts to other µD3TN or ION instances using `aap_config.py`, e.g.:

    ```
    python tools/aap/aap_config.py --tcp <aap-host> <aap-port> \
        --schedule <start> <duration> <rate> \
        <eid> tcpclv3:<host>:<port>
    ```

5. Start the receiving end(s). If you want to receive bundles sent to a µD3TN instance, attach an `aap_receive.py` sink, e.g. via:

    ```
    python tools/aap/aap_receive.py --tcp <aap-host> <aap-port> \
        -a <demux|service-number> -v
    ```

    See the help output of `aap_receive.py` (via `-h`) for additional options to verify received payload data, etc.

    If you want to receive data in ION, you can start an instance of `bprecvfile` or `bpsink` (see ION's documentation).

6. Inject bundles / run your tests. For sending a bundle through µD3TN, you can use `aap_send.py`, e.g. via:

    ```
    python tools/aap/aap_send.py --tcp <aap-host> <aap-port> \
        --agentid <demux|service-number> <dest-eid> "<payload-data>" -v
    ```

    If you want to send a bundle through ION, you may use `bpsource` (see ION's documentation).
