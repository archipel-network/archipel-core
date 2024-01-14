# µD3TN Data Decoder

This sub-project is designed to build a small binary that invokes µD3TN's parsers on data provided via a file and prints the decoded result. It serves multiple purposes, such as gathering information about BPv6/BPv7 bundles and SSP or AAP packets captured from the network, as well as performing fuzz testing on the parsers.

## Build

To build the binary, please enter the following commands from the main project directory:

1. Clean up using `make clean` to ensure a smooth build process.
2. Run `make data-decoder`.
3. After the build process completes successfully, you can find the binary at `./build/posix/ud3tndecode`.

## Invocation


The Data Decoder currently supports parsing the following data types:

- Bundle Protocol version 6 ([RFC 5050](https://www.rfc-editor.org/rfc/rfc5050)) bundles
- Bundle Protocol version 7 ([RFC 9171](https://datatracker.ietf.org/doc/rfc9171/)) bundles
- Application Agent Protocol ([AAP](https://gitlab.com/d3tn/ud3tn/-/blob/master/doc/ud3tn_aap.md)) packets
- CCSDS Space Packet Protocol ([SPP](https://public.ccsds.org/Pubs/133x0b2e1.pdf)) packets

The binary provides a usage summary via `build/posix/ud3tndecode -h` from the main project directory. It shows which argument should be appended depending on the desired parser:


```
Usage: ud3tndecode <datatype> <file>

<datatype> may be one of the following:
	-6 - parse the input file as BPv6 (RFC 5050) bundle
	-7 - parse the input file as BPv7 (RFC 9171) bundle
	-a - parse the input file as AAP packet
	-s - parse the input file as SPP packet
```

There are already predefined example bundles and packets available in the `test/decoder/examples` directory, which can be used as `<file>`. To learn how to create new example bundles or packets, please refer to the [Creating new example files](#creating-new-example-files) section below.

For instance, to invoke the BPv7 parser, you can use `build/posix/ud3tndecode -7 test/decoder/examples/bpv7_test/bpv7_1.bin`. In this command, `-7` indicates that the input file `bpv7_1.bin` located in `test/decoder/examples/bpv7_test/` should be parsed as a BPv7 bundle. Make sure that you run this command from the main project directory.

## Creating new example files

### BPv6 bundle

To create a new example file for a BPv6 bundle, follow these steps:

1. Initialize µD3TN:
- run `build/posix/ud3tn -b 6` from the main project directory (the available arguments can be looked up by invoking `build/posix/ud3tn -h`)

2. Set up "Ncat" as the receiver to capture the bundle's content:
- open another terminal
- install Ncat, if it's not already installed
	- Ncat is part of Nmap
	- to install Nmap on Debian-based Linux destributions (e.g. Debian, Ubuntu, Mint, etc.), please run `sudo apt install nmap`
	- in case you have another Linux distribution, please refer to this [page](https://nmap.org/book/inst-linux.html)
- execute the command `ncat -l localhost 5555 | tee test/decoder/examples/bpv6_test/bpv6_2.bin`
	- `-l` activates listen-mode
	- `localhost` is our IP-address
	- `5555` is the port on which Ncat should listen, make sure that the chosen port is currently not in use (you can check occupied ports with `sudo ss -tulpn`)
	- `tee` ensures that the bundle's content is displayed in the terminal while simultaneously being saved to a file
	- `test/decoder/examples/bpv6_test/` is the location where the example bundle should be stored
	- `bpv6_2.bin` is the desired name for the file

3. Configure contact:
- open yet another terminal
- activate the Python virtual environment by running `make virtualenv` followed by `source .venv/bin/activate`
- run `python3 tools/aap/aap_config.py --schedule 1 3600 100000 dtn://b.dtn/ mtcp:localhost:5555` (please refer to the [µD3TN-documentation](https://gitlab.com/d3tn/ud3tn/-/blob/master/doc/posix_quick_start_guide.md) for detailed explanations of the arguments)

4. Send the bundle:
- use the same terminal as in the previous step, make sure that the Python virtual environment is still activated
- run `python3 tools/aap/aap_send.py dtn://b.dtn/bundlesink 'Hello, world!'`
- the bundle's content should now be visible in the terminal where Ncat was started

5. Delete the MTCP header:
- install a hex-editor, for instance "GHex"
	- to install GHex on Debian-based Linux destributions (e.g. Debian, Ubuntu, Mint, etc.), please enter `sudo apt install ghex`
- open the desktop application and open the newly generated bundle file
- remove the MTCP header by deleting everything before `06`, as this marks the beginning of the primary block of the BPv6 bundle
- save the changes

### BPv7 bundle

The process for creating a new example file for a BPv7 bundle is equivalent to that of a BPv6 bundle. Please follow the steps described in the [BPv6 bundle](#bpv6-bundle) section, replacing all instances of `6` with `7`.

Kindly be aware that the BPv7 bundle begins with the hexadecimal value `0x9F`. Therefore, when eliminating the MTCP header, ensure to delete all content preceding `9F` instead of `07`.

### AAP packet

To create a new example file for an AAP packet, perform the following steps:

Enter the following commands from the main project directory:
- `make virtualenv`
- `source .venv/bin/activate`
- `python`
- `import ud3tn_utils.aap.aap_message as aap_message`

In the following step it depends on the type of message you want to create. The different message types are described in the [documentation](https://gitlab.com/d3tn/ud3tn/-/blob/master/doc/ud3tn_aap.md) and the code is available [here](https://gitlab.com/d3tn/ud3tn/-/blob/master/python-ud3tn-utils/ud3tn_utils/aap/aap_message.py).

1. For messages of the type `ACK`, `NACK` or `PING` we only need to state the message type:
- `msg = aap_message.AAPMessage(aap_message.AAPMessageType.ACK)`
- `msg = aap_message.AAPMessage(aap_message.AAPMessageType.NACK)`
- `msg = aap_message.AAPMessage(aap_message.AAPMessageType.PING)`

2. For messages of the type `REGISTER` or `WELCOME` we need to append the message type and a valid EID:
- `msg = aap_message.AAPMessage(aap_message.AAPMessageType.REGISTER, "dtn://ud3tn.dtn/test")`
- `msg = aap_message.AAPMessage(aap_message.AAPMessageType.WELCOME, "dtn://ud3tn.dtn/test")`

3. For messages of the type `SENDBUNDLE`, `RECVBUNDLE`, `SENDBIBE` or `RECVBIBE` we need to append the message type, a valid EID and a payload:
- `msg = aap_message.AAPMessage(aap_message.AAPMessageType.SENDBUNDLE, "dtn://ud3tn.dtn/test", b"Hello, I am SENDBUNDLE!")`
- `msg = aap_message.AAPMessage(aap_message.AAPMessageType.RECVBUNDLE, "dtn://ud3tn.dtn/test", b"Hello, I am RECVBUNDLE!")`
- `msg = aap_message.AAPMessage(aap_message.AAPMessageType.SENDBIBE, "dtn://ud3tn.dtn/test", b"Hello, I am SENDBIBE!")`
- `msg = aap_message.AAPMessage(aap_message.AAPMessageType.RECVBIBE, "dtn://ud3tn.dtn/test", b"Hello, I am RECVBIBE!")`
Note that the "b" before the payload stands for "binary".

4. For messages of the type `SENDCONFIRM` or `CANCELBUNDLE` we need to state the message type and append a valid Bundle ID:
- `msg = aap_message.AAPMessage(aap_message.AAPMessageType.SENDCONFIRM, bundle_id=2)`
- `msg = aap_message.AAPMessage(aap_message.AAPMessageType.CANCELBUNDLE, bundle_id=2)`

The following steps are equivalent for all types of messages respectively:
- enter `msg.serialize()` (you should now be able to see the packet's content displayed in the terminal)
- enter `with open("test_aap_1.bin", "wb") as f:` (feel free to replace `"test_aap_1.bin"` with the desired file name)
- press TAB on your keyboard
- enter `f.write(msg.serialize())`
- press ENTER twice on your keyboard until a number appears below and "..." changes back to ">>>"

The `"test_aap_1.bin"` file should now appear in the main project directory, where you started the `python` command.

### SPP packet

To create a new example file for an SPP packet, follow these steps:

1. Initialize µD3TN:
- run `build/posix/ud3tn -c tcpspp:localhost,5000,true` from the main project directory
	- `-c` is short for `--cla`, you can look up the available arguments by invoking `build/posix/ud3tn -h`
	- `tcpspp` is our chosen CLA option
	- `localhost` is our IP-address
	- `5000` is the port on which Ncat (see step 2) will listen, make sure that the chosen port is currently not in use (you can check occupied ports with `sudo ss -tulpn`)
	- `true` activates active mode

2. Set up "Ncat" as the receiver to capture the bundle's content:
- open another terminal
- install Ncat, if it's not already installed
	- Ncat is part of Nmap
	- to install Nmap on Debian-based Linux destributions (e.g. Debian, Ubuntu, Mint, etc.), please run `sudo apt install nmap`
	- in case you have another Linux distribution, please refer to this [page](https://nmap.org/book/inst-linux.html)
- run `ncat -l localhost 5000 | tee test/decoder/examples/spp_test/spp_2.bin`
	- `-l` activates listen-mode
	- `localhost` is our IP-address
	- `5000` is the port Ncat is currently listening on (based on the command from step 1)
	- `tee` ensures that the packet's content is displayed in the terminal while simultaneously being saved to a file
	- `test/decoder/examples/spp_test/` is the location where the example packet will be stored
	- `spp_2.bin` is the desired name of the file

3. Configure contact:
- open yet another terminal
- activate the Python virtual environment by running `make virtualenv` followed by `source .venv/bin/activate`
- run `python3 tools/aap/aap_config.py --schedule 1 3600 100000 dtn://b.dtn/ tcpspp:` (please refer to the [µD3TN-documentation](https://gitlab.com/d3tn/ud3tn/-/blob/master/doc/posix_quick_start_guide.md) for a detailed explanation of the arguments)

4. Send the packet:
- use the same terminal as in the previous step, make sure that the Python virtual environment is still activated
- run `python3 tools/aap/aap_send.py dtn://b.dtn/bundlesink 'Hello, world!'`

The packet's content should now be displayed in the terminal where you started Ncat. Unlike the BPv6 bundle, the SPP packet does not include an unwanted header, so step 4 completes the process.

## Using the µD3TN Data Decoder for fuzzing

We use [AFL++](https://aflplus.plus/) for fuzzing. Before we begin, please make sure to download AFL++, either through pulling a Docker image or by going through the manual building process. Both is described [here](https://github.com/AFLplusplus/AFLplusplus/blob/stable/docs/INSTALL.md). Afterwards, please execute the following steps:

1. Clean up:
`make clean`

2. Compile the program to be fuzzed (`data-decoder`) using `afl-gcc-fast`:
`make CC=afl-gcc-fast LD=afl-gcc-fast data-decoder`
(`CC` sets the compiler that shall be used, `LD` sets the linker which combines the individual object files and required libraries into one large overall file)

3. Run the fuzzer:
- for BPv6 bundle: `afl-fuzz -i test/decoder/examples/bpv6_test -o outdata -m 80 -- ./build/posix/ud3tndecode -6 @@`
- for BPv7 bundle: `afl-fuzz -i test/decoder/examples/bpv7_test -o outdata -m 80 -- ./build/posix/ud3tndecode -7 @@`
- for AAP packet: `afl-fuzz -i test/decoder/examples/aap_test -o outdata -m 80 -- ./build/posix/ud3tndecode -a @@`
- for SPP packet: `afl-fuzz -i test/decoder/examples/spp_test -o outdata -m 80 -- ./build/posix/ud3tndecode -s @@`
	- `-i` defines the input directory where the bundles/packets to be fuzzed are stored
	- `-o` determines the output directory where crashes and hangs found by the fuzzer will be saved
	- `-m` stands for memory limit, defining the maximum memory in MB
	- `--` marks the end of the arguments that `afl-fuzz` needs to know about
	- `-6`/`-7`/`-a`/`-s` specifies the parser to be used, as described in section [Invocation](#invocation)

If the fuzzer complains about the system being configured to send core dump notifications to an external utility, you need to temporarily modify the system configurations. Run the following commands:
- `sudo -i` to gain root access
- `echo core >/proc/sys/kernel/core_pattern` to modify the system configurations
- press Ctrl+d to exit from the root access

If the fuzzer complains about the system using on-demand CPU frequency scaling, you need to change the system setting. Run the following commands:
- `sudo -i` to gain root access
- `cd /sys/devices/system/cpu` to change directory
- `echo performance | tee cpu*/cpufreq/scaling_governor` to change the setting
- press Ctrl+d to exit from the root access

After changing these system configurations, just enter the command above again to run the fuzzer. You should now be able to see the fuzzer's working process displayed in your terminal.

4. Let the fuzzer run for some time until it detects a few crashes. You can stop it using Ctrl+c. It is not recommended to run the fuzzer for an extended period, as many crashes are likely to have the same cause.

5. The crash-files can then be found in `./outdata/default/crashes`. These files indicate the input that caused the program to crash. In the next step, we will use one of these crash files to debug the program.

6. Access the [GNU Debugger](https://ftp.gnu.org/old-gnu/Manuals/gdb/html_node/gdb_toc.html) (gdb) from the main project directory:
- make sure you already have `gdb` installed
	- to install gdb on Debian-based Linux destributions (e.g. Debian, Ubuntu, Mint, etc.), please execute `sudo apt install gdb`
	- in case you have another Linux distribution, please refer to this [page](https://www.gdbtutorial.com/tutorial/how-install-gdb)
- enter `gdb build/posix/ud3tndecode`

7. Analyze a specific crash-file in gdb:
`run -7 outdata/default/crashes/<crash-file-name>`
- feel free to replace `-7` with whatever parser you wish to use, make sure to use the same parser that has been used to cause the crash
- replace `<crash-file-name>` with the actual name of the crash-file, its structure should be similar to `id:000000,sig:06,src:000000,time:69,execs:132,op:havoc,rep:2`

8. Debug the program using commands like `backtrace`, `next`, `step`, and more. A list of possible commands is given in this [tutorial](https://www.tutorialspoint.com/gnu_debugger/gdb_commands.htm).
