# AAP 2.0 application interface

AAP 2.0 is an **RPC-like interface**: a command is sent, along with all data needed to process it, to the other party, which responds to it with some return value of a command-specific type.
In extension to that, AAP 2.0 has the following properties:

- Every AAP 2.0 connection has a fixed direction of RPC (starting with calls from the application toward µD3TN) that can be switched once at the start of the connection.
- There can be multiple concurrent connections that can even be registered under the same application endpoint.
- Every endpoint registration has to provide a secret that is shared among all registrations of the same endpoint. If there is already an active registration, the same secret has to be provided or the new (re-)registration will be declined.
- AAP 2.0 uses **Protobuf** as data serialization format. Reasoning: There is wide language support along with good support of the C language and microcontroller environments, and it prevents reinventing an own protocol wire representation plus parser and serializer. Note that we previously evaluated Cap'nProto, which is also a good candidate and especially beneficial due to native RPC support, however, the C language support and microcontroller support is severely lacking, unfortunately.

![The AAP 2.0 protocol state machine](aap20-protocol-state-machine.drawio.svg)

### AAP Message (outer request structure)

All data sent via the AAP 2.0 socket are encapsulated into Protobuf-serialized messages.
Those may either be of the type `AAPMessage` (sent by the Initiator) or `AAPResponse` (sent by the responder after receiving an `AAPMessage`).
Every message is preceded on the wire by a Protobuf [`varint`](https://protobuf.dev/programming-guides/encoding/#varints) encoding its length (using NanoPB's [`PB_ENCODE_DELIMITED`](https://jpa.kapsi.fi/nanopb/docs/reference.html#pb_encode_ex) flag).
After opening the connection, before the initial "Welcome" message, the Server (µD3TN) additionally sends the byte `0x2F` to signal incompatibility to old AAP v1 clients.

An `AAPMessage` may contain one of the different message types that are defined in the following sub-sections.
The `AAPResponse` issued by the peer after receiving an `AAPMessage` always contains a `ResponseStatus`.
The "valid responses" indicated below typically refer to a value of the `ResponseStatus` enum that is part of the `AAPResponse` message.

### Welcome (unacknowledged call)

This message is sent by µD3TN after the Client has successfully connected. It informs the Client of the node ID of the µD3TN node.

- **Issued by:** Server (µD3TN)
- **Valid in connection states:** Connected
- **Valid responses:** none
- **Remarks:** Before the `Welcome` message, the Server (µD3TN) sends the byte `0x2F` to signal incompatibility to old AAP v1 clients.

### ConnectionConfig (call)

As first message to µD3TN, the Client is expected to send a `ConnectionConfig` message, containing the requested endpoint registration and whether the Client wants to send (`is_subscriber == false`) or receive (`is_subscriber == true`) bundle ADUs.
If there is an existing registration for the given endpoint, the `secret` must match the one specified previously by the other Client.
After µD3TN has confirmed successful execution, the configuration is applied. This means also that, if `is_subscriber == true`, the Client is not expected to send data unless µD3TN has handed a bundle ADU (via a `BundleADU` message) to it.

- **Issued by:** Client
- **Valid in connection states:** Connected
- **Valid responses:** `SUCCESS` or `FAILURE`
- **Remarks:** If `is_subscriber` is `true`, the Server (µD3TN) becomes the Initiator and the Client (app.) the Responder. Otherwise, the Client stays the Initiator and the Server is the Responder. The `secret` must be equal to a configured shared secret for managing the µD3TN instance in case any `auth_type != 0` is requested. Otherwise, if the `endpoint_id` is equal to any `endpoint_id` previously registered, the values for `secret` must match. If the previous registration was not associated to a `secret`, it cannot be reused (except the global management secret is provided).

### BundleADU (call)

The main message for exchanging bundle ADUs via AAP 2.0 is `BundleADU`. It transmits all relevant metadata. The payload of the ADU is sent right after the serialized Protobuf message (not in Protobuf format, as Protobuf has limits concerning the binary message size).

- **Issued by:** Initiator
- **Valid in connection states:** Authenticated Active Client (for sending bundles) / Authenticated Passive Client (for receiving bundles)
- **Valid responses:** `SUCCESS` or `FAILURE`
- **Remarks:** Used for sending _and_ receiving ADUs. The payload data are transmitted _after_ the serialized Protobuf message and require dedicated handling within the Client.

### Keepalive (call)

To ensure a lively connection or proactively test its status, when using TCP for AAP 2.0, `Keepalive` messages should be issued by the Initiator (the Client if `is_subscriber == false`, otherwise µD3TN). The other peer should respond with an `AAPResponse` with `RESPONSE_STATUS_ACK`.

- **Issued by:** Initiator
- **Valid in connection states:** all
- **Valid responses:** `ACK`
- **Remarks:** Similar to TCPCLv4, these messages SHOULD be sent when using a TCP connection and when `keepalive_seconds` seconds have elapsed after the last message being received. After `keepalive_seconds * 2` have elapsed without receiving a `KEEPALIVE` message, the connection SHOULD be closed by the Responder.
