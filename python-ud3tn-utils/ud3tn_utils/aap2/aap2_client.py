#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
# encoding: utf-8

import abc
import logging
import random
import socket
import uuid

from google.protobuf.internal.decoder import _DecodeVarint32
from google.protobuf.internal.encoder import _EncodeVarint

from .generated import aap2_pb2


logger = logging.getLogger(__name__)


class AAP2Error(RuntimeError):
    """The base class for errors raised by AAP2Client."""

    pass


class AAP2CommunicationError(AAP2Error):
    """An error raised by AAP2Client when there is a communication error."""

    pass


class AAP2OperationFailed(AAP2Error):
    """An error raised by AAP2Client when uD3TN indicates a failure."""

    pass


class AAP2UnexpectedMessage(AAP2Error):
    """An error raised by AAP2Client when receiving an unexpected message."""

    def __init__(self, *args, message=None):
        super().__init__(*args)
        self.message = message


class AAP2Client(abc.ABC):
    """A context manager class for connecting to uD3TN's AAP 2.0 socket.

    Note:
        This is an abstract base class for two concrete implementations:
        `AAP2UnixClient` for UNIX domain sockets and `AAP2TCPClient` for
        TCP sockets.

    Args:
        address: The address of the remote socket to be used when calling
            `socket.connect()`.

    Attributes:
        socket: The underlying socket object.
        address: The address used to connect to the socket.
        node_eid (str): The local node ID of the connected uD3TN instance.
        agent_id (str): The agent ID assigned to this client.

    """

    def __init__(self, address):
        self.socket = None
        self.address = address
        self.node_eid = None
        self.agent_id = None

    @abc.abstractmethod
    def connect(self):
        """Establish a socket connection to a uD3TN instance and return the
        received welcome message.

        Raises:
            AAP2CommunicationError: On issues receiving data.
            AAP2UnexpectedMessage: If the wrong data was received.
            protobuf.message.DecodeError: If parsing the message fails.
            OSError: If socket communication fails.

        """
        logger.debug("Connected to uD3TN, awaiting WELCOME message...")
        return self._welcome()

    def disconnect(self):
        """Shutdown and close the socket."""
        logger.debug("Terminating connection...")
        self.socket.shutdown(socket.SHUT_RDWR)
        self.socket.close()

    def __enter__(self):
        """Return `self` upon calling `self.connect()` to establish the socket
        connection. See the documentation of the `connect` method.
        """
        self.connect()
        return self

    def __exit__(self, type, value, traceback):
        """Invoke `self.disconnect()` on any raised runtime exception."""
        self.disconnect()

    def _welcome(self):
        """Receive the AAP welcome message and store the node EID of the uD3TN
        instance in `self.node_eid`.
        """
        data = self.socket.recv(1)
        if not data:
            raise AAP2CommunicationError("Connection broke during handshake")
        if data[0] != 0x2f:
            raise AAP2CommunicationError(
                "Did not receive AAP 2.0 magic number 0x2F, but: " +
                hex(data[0])
            )
        msg_welcome = self.receive_msg()
        if msg_welcome.WhichOneof("msg") != "welcome":
            raise AAP2UnexpectedMessage(
                "Expected the 'welcome' oneof field but received: " +
                msg_welcome.WhichOneof("msg"),
                msg_welcome
            )
        self.node_eid = msg_welcome.welcome.node_id
        logger.debug(f"WELCOME message received! ~ EID = {self.node_eid}")
        return msg_welcome.welcome

    @property
    def eid(self):
        """Return the own EID including agent ID."""
        if self.node_eid[0:3] == "ipn":
            prefix, _ = self.node_eid.split(".")
            return f"{prefix}.{self.agent_id}"
        else:
            return f"{self.node_eid}{self.agent_id}"

    @property
    def eid_prefix(self):
        """Return the EID prefix."""
        if self.node_eid[0:3] == "ipn":
            prefix, _ = self.node_eid.split(".")
            return prefix
        else:
            return self.node_eid

    @property
    def is_ipn_eid(self):
        """Return True if the connected node uses the `ipn` EID scheme."""
        return self.node_eid[0:3] == "ipn"

    def _generate_agent_id(self):
        if not self.node_eid:
            return None
        if self.node_eid[0:3] == "dtn":
            return str(uuid.uuid4())
        elif self.node_eid[0:3] == "ipn":
            return str(
                random.randint(1, 4294967295)
            )  # UINT32_MAX for maximum compatibility
        return None

    def configure(self, agent_id=None, subscribe=False, secret=None,
                  auth_type=aap2_pb2.AuthType.AUTH_TYPE_DEFAULT,
                  keepalive_seconds=0):
        """Attempt to configure the connection as specified.

        Args:
            agent_id: The agent identifier to be registered. If None,
                uuid.uuid4() is called to generate one.
            subscribe: Whether to subscribe for bundles (be a passive client)
                or not (be an active client, for sending bundles).
            secret: The AAP 2.0 authentication secret: A value that has to
                match for multiple connections re-using the same agent ID.
                If not specified, a random value will be generated.
            auth_type: The requested AAP 2.0 authentication type. At the
                moment, only AUTH_TYPE_DEFAULT is supported.
            keepalive_seconds: The interval in which keepalive messages are
                expected by uD3TN for active clients or will be sent by uD3TN
                to passive clients. Note that uD3TN will close the connection
                if an active client does not send a keepalive message within
                twice this interval. Set to zero (default value) to disable.
        Return:
            The secret that can be used for registering additional connections.

        Raises:
            AAP2OperationFailed: If uD3TN indicates a processing error.
            protobuf.message.DecodeError: If parsing the response fails.
            OSError: If socket communication fails.

        """
        self.agent_id = agent_id or self._generate_agent_id()
        eid = (
            f"{self.eid_prefix}.{self.agent_id}"
            if self.is_ipn_eid
            else f"{self.eid_prefix}{self.agent_id}"
        )
        if secret is None:
            secret = str(uuid.uuid4())
        config_msg = aap2_pb2.ConnectionConfig(
            auth_type=auth_type,
            is_subscriber=subscribe,
            endpoint_id=eid,
            secret=secret,
            keepalive_seconds=keepalive_seconds,
        )
        logger.debug(f"Sending CONFIGURE message for '{agent_id}'...")
        self.send(aap2_pb2.AAPMessage(config=config_msg))
        response = self.receive_response()
        if (response.response_status !=
                aap2_pb2.ResponseStatus.RESPONSE_STATUS_SUCCESS):
            raise AAP2OperationFailed(
                "The server returned an invalid response status: " +
                str(response.response_status)
            )
        logger.debug("ACK message received!")
        return secret

    def _receive_all(self, count):
        # Receive exactly `count` bytes from socket.
        buf = bytearray(count)
        mv = memoryview(buf)
        i = 0
        while i < count:
            rv = self.socket.recv_into(mv[i:], count - i)
            if rv <= 0:
                return None
            i += rv
        return bytes(buf)

    def _receive_delimited(self):
        """Receive and return a byte array for which the length is indicated
        in a preceding Protobuf varint.
        """
        # Read varint
        PROTOBUF_VARINT_MAX_BYTES = 10  # 64 bits will encode as 10 bytes.
        c = 0
        result = bytearray()
        while c < PROTOBUF_VARINT_MAX_BYTES:
            data = self.socket.recv(1)
            if not data:
                raise AAP2CommunicationError("Connection broke on `recv()`")
            result.append(data[0])
            # No continuation bit set -> exit loop
            if (data[0] & 0x80) == 0:
                break
            c += 1
        if c >= PROTOBUF_VARINT_MAX_BYTES:
            raise AAP2CommunicationError("Invalid varint received")
        data_len = _DecodeVarint32(data, 0)[0]
        # Read and return data
        return self._receive_all(data_len)

    def receive_msg(self):
        """Receive and return the next `AAPMessage`.

        Return:
            The AAPMessage received from uD3TN.

        Raises:
            AAP2CommunicationError: On issues receiving data.
            protobuf.message.DecodeError: If parsing the message fails.
            OSError: If socket communication fails.

        """
        msg_bin = self._receive_delimited()
        msg = aap2_pb2.AAPMessage()
        msg.ParseFromString(msg_bin)
        return msg

    def receive_response(self):
        """Receive and return the next `AAPResponse`.

        Return:
            The AAPResponse received from uD3TN.

        Raises:
            AAP2CommunicationError: On issues receiving data.
            protobuf.message.DecodeError: If parsing the message fails.
            OSError: If socket communication fails.

        """
        msg_bin = self._receive_delimited()
        resp = aap2_pb2.AAPResponse()
        resp.ParseFromString(msg_bin)
        return resp

    def receive_adu(self, adu_msg=None):
        """Receive a bundle ADU (Protobuf with extra payload data).

        Args:
            adu_msg (aap_pb2.BundleADU): If the `BundleADU` message was already
                received, it must be passed using this argument. Otherwise,
                the function will call `receive_msg` to receive the next
                message, which must be of type `BundleADU`.

        Return:
            A tuple of the form (BundleADU, data) containing the received
            `BundleADU` message as metadata describing the ADU payload, and
            the ADU binary payload itself.

        Raises:
            AAP2UnexpectedMessage: If the message received was not of type
                `BundleADU` (if `adu_msg` was set to `None`).
            AAP2CommunicationError: On issues receiving data.
            protobuf.message.DecodeError: If parsing the message fails.
            OSError: If socket communication fails.

        """
        if not adu_msg:
            msg = self.receive_msg()
            if msg.WhichOneof("msg") != "adu":
                raise AAP2UnexpectedMessage(
                    "Expected the 'adu' oneof field but received: " +
                    msg.WhichOneof("msg"),
                    msg
                )
            adu_msg = msg.adu
        bundle_len = adu_msg.payload_length
        bundle_data = self.socket.recv(bundle_len)
        return adu_msg, bundle_data

    def send(self, msg):
        """Serialize and send the provided Protobuf message including header.

        Args:
            msg: The Protobuf message to be sent. Can be, e.g., an `AAPMessage`
                or an `AAPResponse`.

        Raises:
            protobuf.message.EncodeError: If `msg` isn't initialized.
            OSError: If socket communication fails.

        """
        msg_bin = msg.SerializeToString()
        varint_bytes = bytearray()
        _EncodeVarint(lambda b: varint_bytes.extend(b), len(msg_bin))
        self.socket.send(varint_bytes)
        self.socket.send(msg_bin)

    def send_adu(self, adu_msg, bundle_data):
        """Send a bundle ADU (Protobuf with extra payload data).

        Args:
            adu_msg (aap2_pb2.BundleADU): The metadata describing the ADU in
                the form of a `BundleADU` message.
            bundle_data (bytes): The binary payload data to be sent.

        Raises:
            ValueError: If the length of `bundle_data` does not match the
                `payload_data` field of `adu_msg`.
            protobuf.message.EncodeError: If `adu_msg` isn't initialized.
            OSError: If socket communication fails.

        """
        if adu_msg.payload_length != len(bundle_data):
            raise ValueError(
                "Payload length in message does not match length of data"
            )
        self.send(aap2_pb2.AAPMessage(adu=adu_msg))
        self.socket.send(bundle_data)

    def send_response_status(self, status):
        """Send an AAPResponse with the specified status code.

        Args:
            status (aap2_pb2.ResponseStatus): The status code to be sent.

        Raises:
            OSError: If socket communication fails.

        """
        response = aap2_pb2.AAPResponse(response_status=status)
        self.send(response)


class AAP2UnixClient(AAP2Client):
    """A context manager class for connecting to uD3TN's AAP 2.0 Unix socket.

    Args:
        address: The address (PATH) of the remote socket to be used when
            calling `socket.connect()`
    """

    def __init__(self, address='ud3tn.socket'):
        super().__init__(address=address)

    def connect(self):
        self.socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.socket.connect(self.address)
        return super().connect()


class AAP2TCPClient(AAP2Client):
    """A context manager class for connecting to uD3TN's AAP 2.0 TCP socket.

    Args:
        address: The address tuple (HOST, PORT) of the remote socket to be used
            when calling `socket.connect()`
    """

    def __init__(self, address=('localhost', '4242')):
        super().__init__(address=address)

    def connect(self):
        host, service = self.address
        addrinfo = socket.getaddrinfo(host, service,
                                      socket.AF_UNSPEC, socket.SOCK_STREAM)
        for af, socktype, proto, canonname, sa in addrinfo:
            try:
                s = socket.socket(af, socktype, proto)
            except OSError:
                s = None
                continue
            try:
                s.connect(sa)
            except OSError:
                s.close()
                s = None
                continue
            break
        if s is None:
            raise RuntimeError(f"cannot connect to {host}:{service}")
        self.socket = s
        return super().connect()
