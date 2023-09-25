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
    pass


class AAP2CommunicationError(AAP2Error):
    pass


class AAP2OperationFailed(AAP2Error):
    pass


class AAP2UnexpectedMessage(AAP2Error):

    def __init__(self, *args, message=None):
        super().__init__(*args)
        self.message = message


class AAP2Client(abc.ABC):
    """A context manager class for connecting to the AAP socket of a uD3TN
    instance.

    Args:
        socket: A `socket.socket` object
        address: The address of the remote socket to be used when calling
            `socket.connect()`
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
        connection.
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
        """Return the own EID."""
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
        """Return True if the node ID uses the `ipn` scheme."""
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
        Return:
            The secret that can be used for registering additonal connections.
        """
        self.agent_id = agent_id or self._generate_agent_id()
        eid = (
            f"{self.eid_prefix}.{self.agent_id}"
            if self.is_ipn_eid
            else f"{self.eid_prefix}{self.agent_id}"
        )
        self.secret = secret or str(uuid.uuid4())
        config_msg = aap2_pb2.ConnectionConfig(
            auth_type=auth_type,
            is_subscriber=subscribe,
            endpoint_id=eid,
            secret=self.secret,
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
        return self.secret

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
        c = 0
        result = bytearray()
        while c < 10:
            data = self.socket.recv(1)
            if not data:
                raise AAP2CommunicationError("Connection broke on `recv()`")
            result.append(data[0])
            # No continuation bit set -> exit loop
            if (data[0] & 0x80) == 0:
                break
            c += 1
        if c >= 10:
            raise AAP2CommunicationError("Invalid varint received")
        data_len = _DecodeVarint32(data, 0)[0]
        # Read and return data
        return self._receive_all(data_len)

    def receive_msg(self):
        """Receive and return the next `AAPMessage`."""
        msg_bin = self._receive_delimited()
        msg = aap2_pb2.AAPMessage()
        msg.ParseFromString(msg_bin)
        return msg

    def receive_response(self):
        """Receive and return the next `AAPResponse`."""
        msg_bin = self._receive_delimited()
        resp = aap2_pb2.AAPResponse()
        resp.ParseFromString(msg_bin)
        return resp

    def receive_adu(self, adu_msg=None):
        """Receive a bundle ADU (Protobuf with extra payload data)."""
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
        """Serialize and send the provided Protobuf message."""
        msg_bin = msg.SerializeToString()
        varint_bytes = bytearray()
        _EncodeVarint(lambda b: varint_bytes.extend(b), len(msg_bin))
        self.socket.send(varint_bytes)
        self.socket.send(msg_bin)

    def send_adu(self, adu_msg, bundle_data):
        """Send a bundle ADU (Protobuf with extra payload data)."""
        if adu_msg.payload_length != len(bundle_data):
            raise ValueError(
                "Payload length in message does not match length of data"
            )
        self.send(aap2_pb2.AAPMessage(adu=adu_msg))
        self.socket.send(bundle_data)

    def send_response_status(self, status):
        """Send an AAPResponse with the specified status code."""
        response = aap2_pb2.AAPResponse(response_status=status)
        self.send(response)


class AAP2UnixClient(AAP2Client):
    """A context manager class for connecting to the AAP Unix socket of a uD3TN
    instance.

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
    """A context manager class for connecting to the AAP TCP socket of a uD3TN
    instance.

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
