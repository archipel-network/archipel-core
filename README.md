## About

µD3TN (pronounced "Micro-Dee-Tee-En") is a free, lean, and space-tested DTN protocol implementation running on POSIX (plus Linux ;-)).
Though µD3TN is easily portable to further platforms, we currently support only POSIX-compliant systems (former versions also included support for STM32/FreeRTOS platforms).

![](doc/overview.svg)

A general introduction of µD3TN is available at its project web site at https://d3tn.com/ud3tn.html. µD3TN currently implements:

- Bundle Protocol version 6 ([RFC 5050](https://datatracker.ietf.org/doc/html/rfc5050)),
- Bundle Protocol version 7 ([draft version 31](https://datatracker.ietf.org/doc/html/draft-ietf-dtn-bpbis-31)),
- Several Bundle Protocol convergence layers, such as:
  - MTCP ([draft version 0](https://datatracker.ietf.org/doc/html/draft-ietf-dtn-mtcpcl-00)),
  - TCPCLv3 ([RFC 7242](https://datatracker.ietf.org/doc/html/rfc7242)),
  - CCSDS Space Packet Protocol ([SPP](https://public.ccsds.org/Pubs/133x0b2e1.pdf)).

## Pre-compiled binaries

We provide docker images at `registry.gitlab.com/d3tn/ud3tn-docker-images/ud3tn`. Refer to https://gitlab.com/d3tn/ud3tn-docker-images/ for more information.

## Usage

A comprehensive step-by-step tutorial for Linux and POSIX systems is included [in the documentation](doc/posix_quick_start_guide.md). It covers a complete scenario in which two µD3TN instances create a small two-node DTN and external applications leverage the latter to exchange data.

### Start a µD3TN node

For simple setups with just a single node, µD3TN is ready to use with its default settings. For advanced use, the CLI offers at lot of flexibility:

```
Mandatory arguments to long options are mandatory for short options, too.

  -a, --aap-host HOST         IP / hostname of the application agent service
  -b, --bp-version 6|7        bundle protocol version of bundles created via AAP
  -c, --cla CLA_OPTIONS       configure the CLA subsystem according to the
                                syntax documented in the man page
  -e, --eid EID               local endpoint identifier
  -h, --help                  print this text and exit
  -l, --lifetime SECONDS      lifetime of bundles created via AAP
  -m, --max-bundle-size BYTES bundle fragmentation threshold
  -p, --aap-port PORT         port number of the application agent service
  -r, --status-reports        enable status reporting
  -s, --aap-socket PATH       path to the UNIX domain socket of the application agent service
  -S, --aap2-socket PATH      path to the UNIX domain socket of the experimental AAP 2.0 service
  -u, --usage                 print usage summary and exit

Default POSIX invocation: ud3tn \
  -b 7 \
  -c "tcpclv3:*,4556;tcpspp:*,4223,false,1;smtcp:*,4222,false;mtcp:*,4224" \
  -e dtn://ud3tn.dtn/ \
  -l 86400 \
  -m 18446744073709551615 \
  -s $PWD/ud3tn.socket \
  -S $PWD/ud3tn.aap2.socket
```

The AAP interface can use either a UNIX domain socket (`-s` option) or bind to a TCP address (`-a` and `-p` options).
Examples for `CLA_OPTIONS` are documented in the [man page](doc/ud3tn.1),
which can be viewed with `man --local-file doc/ud3tn.1`.
Default arguments and internal settings such as storage, routing, and connection parameters can be adjusted in [`include/ud3tn/config.h`](include/ud3tn/config.h).

### Configure contacts with other µD3TN / BP nodes

µD3TN performs its bundle forwarding decisions based on _contacts_, which are associated with a specific bundle _node_. Each instance accepts bundles addressed to `dtn://<ud3tn-node-name>/config` or `ipn:<ud3tn-node-number>.9000` (by default, only via AAP) and parses them according to the specification documented at [`doc/contacts_data_format.md`](doc/contacts_data_format.md). To sum it up, a contact can be used to configure:

- start and end time (optional),
- data rate (optional),
- reliability of the contact (optional), and
- whether the bundle node can reach other nodes during this specific contact (list of EID, optional).

Nodes themselves can be configured (added / removed) via the same syntax and interface.

This repository includes convenient python tools that can be used after [preparing the python environment](doc/python-venv.md) to [configure contacts](tools/aap/aap_config.py).

### Leverage AAP to make applications delay and disruption tolerant

Once a µD3TN enabled DTN network has been created, applications can leverage the custom Application Agent Protocol (AAP) to interact with it. Applications typically use AAP to:

- register themselves at a µD3TN instance with a local identifier,
- inject bundles (hand over a payload and a destination EID to µD3TN, µD3TN then creates a corresponding bundle and tries to forward / deliver it), and
- listen for application data addressed to their identifier.

The protocol is defined at [`doc/ud3tn_aap.md`](doc/ud3tn_aap.md). Like above, there are [dedicated python scripts for various tasks](tools/aap). Python bindings for AAP are available under the name [`ud3tn-utils`](https://pypi.org/project/ud3tn-utils/) in the Python package manager PyPI.

µD3TN v0.13.0 also contains an experimental preview of AAP 2.0, the next generation application protocol. In the future, AAP 2.0 will support controlling bundle forwarding decisions and links to other nodes. In the current version, only basic ADU send/receive functionality is implemented, matching the feature set of AAP 1.0 (extended by more ADU metadata). Please refer to the [AAP 2.0 Overview](doc/aap20.md) and the [AAP 2.0 Protobuf Description](components/aap2/aap2.proto) for more details.

## Develop

### Build

This project uses git submodules to manage some code dependencies. 
Use the `--recursive` option if you `git clone` the project or run
`git submodule init && git submodule update` at a later point in time.

#### POSIX-compliant operating systems

1. Install or unpack the build toolchain
   - Install GNU `make`, `gcc` and `binutils`.
   - For building with Clang, additionally install a recent version of `clang` and `llvm`.

2. Configure the local build toolchain in `config.mk` (optional for most systems)
   - Copy `config.mk.example` to `config.mk`.
   - Adjust `TOOLCHAIN` if you want to build with Clang.
   - Adjust `TOOLCHAIN_POSIX` if your toolchain installation is not included in your `$PATH`

3. Run `make run-posix` to build and execute µD3TN on your local machine.
   - You can find the µD3TN binary file in `build/posix/ud3tn`. To just build it, you can also run `make posix` or `make` (the latter building the library files as well).
   - Note that on some systems, such as BSD flavors, you may need to explicitly call GNU Make using the `gmake` command. In this case, just substitute all calls to `make` in the documentation by `gmake`.
   - Some build-time options (e.g., building with sanitizers) can be easily specified as arguments to `make`. See `config.mk.example` for the values you can specify. Example: `make sanitize=yes`

#### Nix-based build and development (x86_64-linux)

1. [Install the nix package manager](https://nixos.org/download)
2. [Enable flake support](https://nixos.wiki/wiki/Flakes)
    - *Temporary:* Add `--experimental-features 'nix-command flakes'` when using any nix commands
    - *Permanent:* Add `experimental-features = nix-command flakes` to `~/.config/nix/nix.conf` or `/etc/nix/nix.conf`

Most common nix commands are:

- Build & run ud3tn:

    ```sh
    nix run '.?submodules=1'
    ```
- Build individual packages:

    ```sh
    nix build '.?submodules=1#ud3tn'
    nix build '.?submodules=1#pyd3tn'
    nix build '.?submodules=1#python-ud3tn-utils'
    ```

- Load a development environment with all packages and dependencies:

    ```sh
    nix develop '.?submodules=1'
    ```

    After the development environment has been activated, all development dependencies are fulfilled in order to be able to execute all other described debug and build commands.

#### Library

Beside the µD3TN daemon binary, two types of library can be built using `make posix-lib` or `make`:

- `build/posix/libud3tn.so`: a dynamic library (shared object) containing all but the daemon functions.
- `build/posix/libud3tn.a`: a _thin_ static library providing the same functionality. This only _references_ the `component.a` files in the `build` directory and is intended to statically link µD3TN into other projects. The preferred way to do this is to include µD3TN as part of your project's source tree (e.g. using `git subtree` or `git submodule`).

### Test

The µD3TN development is accompanied by extensive testing. For this purpose, you should install `gdb` and a recent version of Python 3 (>= 3.7), plus the, `venv`, `pip`, and `setuptools` packages for your Python version. Our test suite covering static analysis, unit, and integration tests is documented in [`doc/testing.md`](doc/testing.md).

### Contribute

Contributions in any form (e.g., bug reports, feature, or merge requests) are very welcome! Please have a look at [CONTRIBUTING.md](CONTRIBUTING.md) first for a smooth experience. The project structure is organized as follows:

```
.
├── components             C source code
├── include                C header files
├── external               3rd party source code
├── generated              generated source code (e.g. for Protobuf)
├── test                   various test routines
├── doc                    documentation
├── mk                     make scripts
├── dockerfiles            Templates for creating Docker images
├── python-ud3tn-utils     Python bindings for AAP
├── pyd3tn                 Python implementation of several DTN protocols
└── tools                  various utility scripts
```

The entry point is implemented in [`components/daemon/main.c`](components/daemon/main.c).

## License

This work, encompassing all files except the ones located in `external/`, is dual-licensed under BSD 3-Clause and Apache 2.0. You can choose between one of these licenses if you use this work.

The external libraries shipped with µD3TN are subject to other licenses, see [`LICENSE-3RD-PARTY.txt`](external/LICENSE-3RD-PARTY.txt) for the corresponding legal information.

`SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0`

## Ecosystem

- [`ud3tn-utils`](https://pypi.org/project/ud3tn-utils/) is a Python package that provides bindings for µD3TN's [Application Agent Protocol](doc/ud3tn_aap.md).
- [`aap.lua`](tools/aap.lua) is a Wireshark dissector for µD3TN's [Application Agent Protocol](doc/ud3tn_aap.md). It can be installed by copying it into one of the Lua script folders listed in the Wireshark GUI at `Help > About Wireshark > Folders`.
- [`pyD3TN`](https://pypi.org/project/pyD3TN/) is a Python package that provides implementations of several DTN related RFCs.
- [`aiodtnsim`](https://gitlab.com/d3tn/aiodtnsim) is a minimal framework for performing DTN simulations based on Python 3.7 and asyncio.
- [`dtn-tvg-util`](https://gitlab.com/d3tn/dtn-tvg-util) is a Python package simplifying the analysis and simulation of DTNs based on time-varying network graphs.

## See also

- [RFC 4838](https://datatracker.ietf.org/doc/html/rfc4838) for a general introduction about DTN networks.
- [ION](https://sourceforge.net/projects/ion-dtn/): NASA's bundle protocol implementation that has been successfully demonstrated to be interoperable with µD3TN.
