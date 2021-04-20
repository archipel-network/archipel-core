---
title: |
    µD3TN POSIX Quick Start Guide
documentclass: scrartcl
lang: en-US
toc: false
numbersections: true
---

Hi there and thanks for your interest in µD3TN!
This document aims to provide some pointers for getting multiple µD3TN instances up and running and quickly sending around some bundles.

# Prerequisites

It is assumed all following commands are launched on a **POSIX** or **Linux** system.
A VM or Docker container will work for this purpose, too.

## Dependencies

Please install all dependencies stated in the [`README.md` file](https://gitlab.com/d3tn/ud3tn/-/blob/master/README.md) for the version you have obtained.
At least an up-to-date **C build toolchain**, **GNU Make**, and the **GNU Binutils** are always needed.
Apart from that, you need an up-to-date version of **Python 3** for the µD3TN tools and **Git** for working with the code repository.

## Getting the code

Apart from installing the dependencies, you need the µD3TN code. You can pull it from Git as follows (replace `vX.Y.Z` by the [version](https://gitlab.com/d3tn/ud3tn/-/releases) you want to use):

```
git clone https://gitlab.com/d3tn/ud3tn.git
cd ud3tn
git checkout vX.Y.Z
git submodule update --init --recursive
```

## Build and run µD3TN

Build µD3TN as follows:

```
make posix
```

Given your toolchain is working properly, the command should complete without an error code.

You can now run µD3TN as follows:

```
make run-posix
```

This will launch a new instance of µD3TN with the default parameters; you will see some output that µD3TN is starting up. When everything is set up, the daemon will continue running and can be terminated as usual via `SIGINT` (Ctrl+C) or `SIGTERM`.

**Note:** For modifying µD3TN's command line parameters, you can directly launch the built binary `build/posix/ud3tn`. Some information on the parameters can be retrieved via `-h` and from the [man page](https://gitlab.com/d3tn/ud3tn/-/blob/master/doc/ud3tn.1) via `man -l doc/ud3tn.1`.

## Python dependencies

For getting the Python tools working, you have two options:

1. If you have already pulled the code of a specific version it is probably easiest to use the [**development setup**](https://gitlab.com/d3tn/ud3tn/-/blob/master/doc/python-venv.md). Just run `make virtualenv` in the µD3TN directory and it will create a `.venv` subdirectory containing a full [Python virtual environment](https://www.python.org/dev/peps/pep-0405/) for development. You can activate the virtual environment in your shell of choice via `source .venv/bin/activate`.

2. If you don't want to create a new virtual environment or have troubles with the development setup, you can use the [**PyPI packages**](https://pypi.org/user/d3tn/). You can, of course, also install them in a virtual environment or use a Python package manager of your choice. Installation via `pip` is performed as follows (replace `vX.Y.Z` by the µD3TN version you are using): `pip install pyd3tn==vX.Y.Z ud3tn-utils==vX.Y.Z`

Everytime we run a `python` command in the following section(s), these dependencies should be available (i.e., if you are using a virtual environment, it should be _activated_).

# Simple two-instance message-passing

In this scenario we want to spin up two µD3TN instances representing two nodes, connect them via the [minimal TCP convergence layer adapter](https://tools.ietf.org/html/draft-ietf-dtn-mtcpcl) (MTCP CLA), and send a [BPv7 bundle](https://tools.ietf.org/html/draft-ietf-dtn-bpbis) from an application connected to one instance to the second instance.

## Start µD3TN instances

We will use two µD3TN instances with two different identifiers for the node (Node IDs; note that a Node ID is also a valid DTN endpoint identifier / EID):
- Instance `A` with Node ID `dtn://a.dtn`, with MTCP listening on TCP port `4224` and application agent listening on port `4242`
- Instance `B` with Node ID `dtn://b.dtn`, with MTCP listening on TCP port `4225` and application agent listening on port `4243`

### Step 1 {-}

Open a new shell in the µD3TN base directory and launch instance `A` as follows:

```
build/posix/ud3tn --eid dtn://a.dtn --bp-version 7 --aap-port 4242 \
    --cla "mtcp:*,4224"
```

This will tell µD3TN to run with Node ID `dtn://a.dtn`, generate bundles using version `7` (BPbis / BPv7), use TCP and port `4242` for [AAP](https://gitlab.com/d3tn/ud3tn/-/blob/master/doc/ud3tn_aap.md) and listen on TCP port `4224` with the MTCP CLA.

### Step 2 {-}

Open _another_ shell and launch instance `B` as follows:

```
build/posix/ud3tn --eid dtn://b.dtn --bp-version 7 --aap-port 4243 \
    --cla "mtcp:*,4225"
```

You see that we have just incremented the port numbers by one to avoid a collision.

Now, both µD3TN instances are up and running. Let's continue with the next step!

## Configure µD3TN instances

µD3TN needs to be _configured_ to know which DTN nodes are reachable during which time intervals (_contacts_) and with which data rates. This configuration occurs at runtime and is not persistent at the moment. Thus, after each start you have to re-configure µD3TN.

Configuration occurs via DTN bundles containing a specific [configuration message format](https://gitlab.com/d3tn/ud3tn/-/blob/master/doc/contacts_data_format.md) destined for the configuration endpoint ID `<µD3TN Node ID>/config`.
In its default configuration (that can be changed via build flags), for security reasons, µD3TN only accepts such bundles via the local AAP socket.

To simplify the configuration process, we provide a Python tool for configuration.
Make sure you have activated the Python virtual environment or made available the `pyd3tn` and `ud3tn-utils` packages in another way.

### Step 3 {-}

Open a third shell and configure a contact from `A` to `B`:

```
python tools/aap/aap_config.py --tcp localhost 4242 --dest_eid dtn://a.dtn \
    --schedule 1 3600 100000 \
    dtn://b.dtn mtcp:localhost:4225
```

This specifies that `aap_config.py` shall connect via AAP to `localhost` on TCP port `4242` and issue a configuration command to the µD3TN daemon with Node ID `dtn://a.dtn`.

The contact is defined by the `--schedule` argument: It starts `1` second from now (the time the command has been invoked), runs for `3600` seconds, and has an expected transmission rate of `100000` bytes per second.

The last two arguments specify with which DTN Node ID the specified contact(s) are expected and via which convergence layer address the node is reachable.

See `python tools/aap/aap_config.py -h` for more information on the command line arguments of the script.

You will see that instance `A` will attempt to connect to instance `B`, one second after this command has been issued. Instance `B` will accept the connection.

Note the following:
- Instance `B` will not have a possibility to send bundles to instance `A` - the connectivity is unidirectional as MTCP is a unidirectional CLA. For sending bundles the other way, you need to configure a contact in instance `B` and provide the port over which instance `A` is reachable.
- You may, of course, configure a contact at a later point in time (e.g., by setting the first parameter of `--schedule` to `60` it will start a minute after issuing the command). µD3TN will queue all bundles received for a given node for which it knows about future contacts. This means you always have to configure the contacts first, but they can start at an arbitrary time in the future.

## Attach a receiving application

### Step 4 {-}

For sending bundles over the now-established link, first attach a receiver to instance `B`:

```
python tools/aap/aap_receive.py --tcp localhost 4243 --agentid bundlesink
```

This requests µD3TN instance `B` via AAP on TCP port `4243` to register a new application with agent ID `bundlesink` (the endpoint ID will be `dtn://b.dtn/bundlesink`). If the registration is successful (which it should be), `aap_receive.py` will wait for incoming bundles on that socket connection and print their payload to the console.

## Send a bundle

Now we are finally ready to send a bundle!

### Step 5 {-}

Open another terminal and send a new bundle via AAP to instance `A`:

```
python tools/aap/aap_send.py --tcp localhost 4242 \
    dtn://b.dtn/bundlesink \
    'Hello, world!'
```

This asks µD3TN instance `A` via AAP on TCP port `4242` to create and send a new bundle to `dtn://b.dtn/bundlesink` with payload data `Hello, world!`.

You should see the received bundle appear in the output of `aap_receive.py`.

# Closing remarks

Although the setup may seem complex at first, it is overall quite straight-forward to interact with µD3TN instances.
It is recommended to take a look at code of the provided Python tools and libraries, which can serve as a starting point to develop own integrations.

If you have any questions or want to contribute, just send us a mail via [contact@d3tn.com](mailto:contact@d3tn.com)!
