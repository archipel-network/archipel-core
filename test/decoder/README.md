# µD3TN Data Decoder

This sub-project builds a small binary invoking µD3TN's parsers on data provided via a file and printing the decoded result.
It can be used to gather information about bundles, CLA or AAP packets captured from the network or to perform fuzz testing on the parsers.

## Build

To build the binary, invoke `make data-decoder` from the main project directory.
You can find the binary in `build/posix/ud3tndecode`.

## Invocation

The binary provides a usage summary via the `-h` command line argument. Please refer to its output.
