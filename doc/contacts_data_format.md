# µD3TN Contacts Data Format

Configuration can be performed via bundles to the µD3TN config endpoint.
If the `dtn` EID scheme is used, the config endpoint is `dtn://<ud3tn-node-name>/config` by default. If the `ipn` EID scheme is used, the config endpoint is `ipn:<ud3tn-node-number>.9000` by default.
Using this mechanism, connected nodes as well as upcoming contacts to them can be configured.

**Note:** By default, the configuration endpoint drops bundles received from external DTN nodes, i.e., configuration can only be performed from an application attached via AAP.
To allow remote configuration, run ud3tn with `--allow-remote-config`.

## Description

The used data format is as follows:

```
<COMMAND><NODE_ID_STRING><,RELIABILITY><:CLA_ADDRESS_STRING><:REACHABLE_EID_LIST><:CONTACT_LIST>;
```

There are three basic values for `COMMAND` that can be used to configure µD3TN via this interface:
  * `1` representing **ADD**: Create the provided node if it does not exist and assign the associated endpoints and contacts to it.
  * `2` representing **REPLACE**: Delete the node if it exists already, then re-create it with the provided data.
  * `3` representing **DELETE**: Delete the given endpoints or contacts from the given node, or delete the node altogether if no endpoints or contacts are provided.

`NODE_ID_STRING` is always mandatory. All strings shall be enclosed in parentheses, e.g., `(dtn://ud3tn2.dtn/)` is a valid node ID string.

The `RELIABILITY` value is optional and shall be an integer number between 100 and 1000 and represent the expected likelihood that a future contact with the given node will be observed, divided by 1000.0.

`CLA_ADDRESS_STRING` is mandatory when creating a node and uses the same string representation as the node ID and consists of the convergence layer adapter and the node address, e.g., `(tcpclv3:127.0.0.1:1234)`.

`REACHABLE_EID_LIST` is optional and shall be enclosed in square brackets and contain comma-separated EIDs in the same string representation as the node ID.

`CONTACT_LIST` is optional and shall also be enclosed in square brackets. It contains comma-separated contacts in the following format:

```
{<START_DTN_TIME>,<END_DTN_TIME>,<DATA_RATE>,<REACHABLE_EID_LIST>}
```

The `START_DTN_TIME` and `END_DTN_TIME` shall be integer DTN timestamps in seconds. The `DATA_RATE` shall be an integer number representing the expected transmission rate in bytes per second. The `REACHABLE_EID_LIST` uses the same format as the one for the node and is appended only for the specific contact.

Please note that although the numbers are parsed as unsigned 64-bit integers, internal processing currently assumes `INT32_MAX - 1` (bytes) to be the maximum valid capacity (volume) of a contact, given by the product of contact duration and data rate. If the calculated capacity value is larger, the contact is assumed to have "infinite capacity", meaning that an arbitrary number of bundles of arbitrary size can be scheduled for it.

## Examples

The following lines show examples for configuration data sent to µD3TN.

```
1(dtn://ud3tn2.dtn/):(mtcp:127.0.0.1:4223)::[{1401519306972,1401519316972,1200,[(dtn://89326/),(dtn://12349/)]},{1401519506972,1401519516972,1200,[(dtn://89326/),(dtn://12349/)]}];
1(dtn://ud3tn2.dtn/)::[(dtn://18471/),(dtn://81491/)]:[{1401519406972,1401819306972,1200}];
2(dtn://ud3tn2.dtn/):(mtcp:127.0.0.1:4223):[(dtn://89326/),(dtn://12349/)];
3(dtn://ud3tn2.dtn/);
1(dtn://13714/):(tcpspp:):[(dtn://18471/),(dtn://81491/)];
1(dtn://13714/),333;
```
