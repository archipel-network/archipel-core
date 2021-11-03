# µD3TN Application Agent Protocol (AAP)

The µD3TN Application Agent Protocol (AAP) allows applications to register a custom (sub-)EID with µD3TN and send and receive bundles via this EID.
Communication takes place over a plain TCP connection. The default port is 4242. This document describes version 1 (v1) of the protocol.

## Message Format

All AAP messages consist of a 1-byte combined version and type code, followed by type-specific data.
The "version-and-type-byte" is formed of the most significant nibble representing the version and the least significant nibble representing the type as follows (MSB first):

```
 76543210
+--------+
|0001TTTT|
+--------+
```

The 4-bit version code is always set to `0x1` in the v1 protocol described here.

The following type codes are possible:

* `0x0` positive acknowledgment (ACK)
* `0x1` negative acknowledgment (NACK)
* `0x2` EID registration request (REGISTER)
* `0x3` Bundle transmission request (SENDBUNDLE)
* `0x4` Bundle reception message (RECVBUNDLE)
* `0x5` Bundle transmission confirmation (SENDCONFIRM)
* `0x6` Bundle cancellation request (CANCELBUNDLE)
* `0x7` Connection establishment notice (WELCOME)
* `0x8` Connection liveliness check (PING)
* `0x9` BIBE Bundle transmission request (SENDBIBE)
* `0xA` BIBE Bundle reception message (RECVBIBE)
* `0xB`-`0xF` RESERVED (for future use)

Numbers contained in type-specific data fields are always represented in network byte order.

### Acknowledgments (ACK, NACK)

The *ACK* and *NACK* messages do not contain additional data. They acknowledge or disacknowledge successful processing of a request.

*ACK*:

```
+--------+
|00010000|
+--------+
```

*NACK*:

```
+--------+
|00010001|
+--------+
```

### EID registration request (REGISTER)

The *REGISTER* message can only be sent by the client and requests that µD3TN associates the current connection with a specific EID.
The message is encoded as follows:

```
+--------+--------+--------+--------+--------+--------+--------+--------+
|00010010| EID length (16) | sub-EID (variable-length)        ...       |
+--------+--------+--------+--------+--------+--------+--------+--------+
```

The EID contained within this message is *not* the complete EID that the application will be associated to, but the agent identifier, i.e. only the last part of the EID. The first part of the EID (as communicated in the *WELCOME* message) is configured in µD3TN and cannot be altered.

**Note:** In the case of a `dtn` scheme EID, the sub-EID comprises the `demux` part as specified in the [BPv7 document](https://datatracker.ietf.org/doc/html/draft-ietf-dtn-bpbis-31#section-4.2.5.1.1).
In the case of an `ipn` scheme EID, the sub-EID is the service number in string format. See also the [BPv7 document](https://datatracker.ietf.org/doc/html/draft-ietf-dtn-bpbis-31#section-4.2.5.1.2).

### Bundle transmission request (SENDBUNDLE)

The *SENDBUNDLE* message can only be sent by the client (the application). It contains the destination EID and the payload data as follows:

```
+--------+--------+--------+--------+--------+--------+--------+--------+
|00010011| EID length (16) | Destination EID (variable-length)    ...   |
+--------+--------+--------+--------+--------+--------+--------+--------+
| Payload length (64)                                                   |
+--------+--------+--------+--------+--------+--------+--------+--------+
| Payload data (variable-length)                                        |
+--------+--------+--------+--------+--------+--------+--------+--------+
```

### Bundle reception message (RECVBUNDLE)

The *RECVBUNDLE* message can only be sent by the server (µD3TN). It is encoded in the same manner as the *SENDBUNDLE* message but instead contains the bundle's source EID:

```
+--------+--------+--------+--------+--------+--------+--------+--------+
|00010100| EID length (16) | Source EID (variable-length)         ...   |
+--------+--------+--------+--------+--------+--------+--------+--------+
| Payload length (64)                                                   |
+--------+--------+--------+--------+--------+--------+--------+--------+
| Payload data (variable-length)                                        |
+--------+--------+--------+--------+--------+--------+--------+--------+
```

### Bundle transmission confirmation (SENDCONFIRM)

The *SENDCONFIRM* message communicates that a bundle was accepted and queued for transmission. It can only be sent by the server (µD3TN) and contains a numeric 64-bit bundle identifier uniquely identifying the queued bundle. This can be used in a *CANCELBUNDLE* message to cancel the transmission request.

```
+--------+--------+--------+--------+--------+--------+--------+--------+--------+
|00010101| Bundle ID (64)                                                        |
+--------+--------+--------+--------+--------+--------+--------+--------+--------+
```

### Bundle cancellation request (CANCELBUNDLE)

This message requests cancellation of a queued bundle. It can only be sent by the client (the application) and is encoded as follows:

```
+--------+--------+--------+--------+--------+--------+--------+--------+--------+
|00010110| Bundle ID (64)                                                        |
+--------+--------+--------+--------+--------+--------+--------+--------+--------+
```

### Connection establishment notice (WELCOME)

This message is only sent by the server (µD3TN), once at the start of every connection. It communicates µD3TN's own EID without the agent ID. In the case of
a `dtn` scheme EID, this comprises the first part of the EID before the slash.
In the case of an `ipn` scheme EID, the service number is `0`.

```
+--------+--------+--------+--------+--------+--------+--------+--------+
|00010111| EID length (16) | µD3TN EID (variable-length)          ...   |
+--------+--------+--------+--------+--------+--------+--------+--------+
```

### Connection liveliness check (PING)

This message can be sent by either the client (the application) or µD3TN and is always answered with an *ACK* message. In which intervals the message should be sent is defined by the originating instance.

*PING*:

```
+--------+
|00011000|
+--------+
```

### BIBE Bundle transmission request (SENDBIBE)

The *SENDBIBE* message can only be sent by the so called upper layer or a client (application). It contains the destination EID and a BIBE Protocol Data Unit as payload data as follows:

```
+--------+--------+--------+--------+--------+--------+--------+--------+
|00011001| EID length (16) | Destination EID (variable-length)    ...   |
+--------+--------+--------+--------+--------+--------+--------+--------+
| Payload length (64)                                                   |
+--------+--------+--------+--------+--------+--------+--------+--------+
| Payload data (variable-length)                                        |
+--------+--------+--------+--------+--------+--------+--------+--------+
```

### BIBE Bundle reception message (RECVBIBE)

The *RECVBIBE* message can only be sent by the so called lower layer. It is encoded in a similiar manner as the *SENDBIBE* message but instead contains the bundle's source EID:

```
+--------+--------+--------+--------+--------+--------+--------+--------+
|00011010| EID length (16) | Source EID (variable-length)         ...   |
+--------+--------+--------+--------+--------+--------+--------+--------+
| Payload length (64)                                                   |
+--------+--------+--------+--------+--------+--------+--------+--------+
| Payload data (variable-length)                                        |
+--------+--------+--------+--------+--------+--------+--------+--------+
```

## Protocol Operation

### Connection establishment

For establishing a connection to µD3TN (the server), the application (the client) opens a new TCP connection to the AAP port configured in µD3TN. µD3TN responds to the newly opened connection with a *WELCOME* message.

### EID registration

The application can only send and receive bundles if it registers itself under an endpoint identifier (EID). The full EID is composed of two parts: the µD3TN EID prefix and the application-defined second part of the EID.
The first part is communicated by µD3TN in the *WELCOME* message at the start of the connection. The second part needs to be registered by the application. Only one EID can be registered per TCP connection. For registering an EID, a *REGISTER* message is sent by the application to µD3TN. µD3TN responds with either a positive (*ACK*) or a negative (*NACK*) acknowledgment. If the EID has not been registered by another client currently connected and µD3TN can allocate the necessary resources, registration will succeed.

Re-registration and termination of registrations is possible. If a second *REGISTER* message is sent during an ongoing connection, the existing registration is replaced by the new one, if successful. A registration is deleted by either terminating the TCP connection or by sending a *REGISTER* message with an empty (zero-length) EID. Then, no bundles can be sent or received.

### Bundle transmission

If EID registration has been performed successfully, bundles can be transmitted via the connection. µD3TN handles bundle creation, thus, the client only needs to transmit the destination EID as well as the payload data via a *SENDBUNDLE* message.
If µD3TN could create and queue the bundle for transmission, it sends a *SENDCONFIRM* message back to the client, containing the bundle identifier. If the bundle creation failed or no EID registration was performed beforehand, a *NACK* message is sent back to the client.

### Bundle reception

If EID registration has been performed successfully and a bundle destined to the client application is received by µD3TN, it will issue a *RECVBUNDLE* message to the client. This message contains the source EID and the payload data of the received bundle.

### Bundle cancellation

While a bundle is queued for transmission inside µD3TN, the planned transmission(s) can be cancelled by the client. For that purpose, a *CANCELBUNDLE* message can be sent to µD3TN. µD3TN will respond with either an *ACK* or a *NACK* message, depending on whether the bundle could be cancelled successfully. Upon cancellation, the bundle is dropped completely by µD3TN.

### Connection check

For long-lasting connections, it has to be ensured that the operating system does not assume the connection to be dead and closes it automatically. For that purpose, the client or µD3TN can (e.g. periodically) send a *PING* message which is answered by the receiving side with an *ACK* message.

### BIBE Bundle transmission

If the upper layer has successfully registered at the lower layer, BIBE Bundles can be sent. For this, the upper layer transmits the destination EID as well as the payload data (here the BPDU containing the bundle that should be encapsulated) via a *SENDBIBE* message to the lower layer.
If the lower layer could create and queue the bundle for transmission, it sends a *SENDCONFIRM* message back to the upper layer, containing the bundle identifier. If the bundle creation failed or no EID registration was performed beforehand, a *NACK* message is sent back to the upper layer.   
For more information regarding the operation of BIBE, especially regarding the definitions of the upper and lower layer and the registration process, please have a look at the BIBE docs.

### BIBE Bundle reception

If the upper layer has successfully registered at the lower layer and a BIBE bundle addressed to the lower layer is received, the lower layer will issue a *RECVBIBE* message to the upper layer. This message contains the source EID and a payload, which is the BIBE Protocol Data Unit of the received BIBE Bundle.

### Backward compatibility
The addition of the SENDBIBE and RECVBIBE message types did not warrant an increase of the version number, as no substantial changes to the way AAP works have been introduced. It also did not introduce behavior that would break backward compatibility to µD3TN implementations without support for SEND-/RECVBIBE messages, as these messages will simply be discarded upon reception. In a network featuring both µD3TN implementations with and without BIBE support, one should thus be cautious when using Bundle-in-Bundle Encapsulation, as BIBE Bundles may get dropped by older µD3TN instances.