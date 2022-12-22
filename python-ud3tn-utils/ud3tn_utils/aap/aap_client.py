#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
# encoding: utf-8

import abc
import logging
import random
import socket
import uuid

from .aap_message import AAPMessage, AAPMessageType, InsufficientAAPDataError


logger = logging.getLogger(__name__)


class AAPClient(abc.ABC):
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
        msg_welcome = self.receive()
        assert msg_welcome.msg_type == AAPMessageType.WELCOME
        logger.debug(f"WELCOME message received! ~ EID = {msg_welcome.eid}")
        self.node_eid = msg_welcome.eid
        return msg_welcome

    @property
    def eid(self):
        """Return the own EID."""
        if self.node_eid[0:3] == "ipn":
            prefix, _ = self.node_eid.split(".")
            return f"{prefix}.{self.agent_id}"
        else:
            return f"{self.node_eid}{self.agent_id}"

    def _generate_eid(self):
        if not self.node_eid:
            return None
        if self.node_eid[0:3] == "dtn":
            return str(uuid.uuid4())
        elif self.node_eid[0:3] == "ipn":
            return str(
                random.randint(1, 4294967295)
            )  # UINT32_MAX for maximum compatibility
        return None

    def register(self, agent_id=None):
        """Attempt to register the specified agent identifier.

        Args:
            agent_id: The agent identifier to be registered. If None,
                uuid.uuid4() is called to generate one.
        """
        self.agent_id = agent_id or self._generate_eid()
        logger.debug(f"Sending REGISTER message for '{agent_id}'...")
        msg_ack = self.send(
            AAPMessage(AAPMessageType.REGISTER, self.agent_id)
        )
        assert msg_ack.msg_type == AAPMessageType.ACK
        logger.debug("ACK message received!")

    def ping(self):
        """Send a PING message via AAP and returns the ACK message (e.g. for
        keepalive purposes).
        """
        msg_ack = self.send(AAPMessage(AAPMessageType.PING))
        assert msg_ack.msg_type == AAPMessageType.ACK
        return msg_ack

    def receive(self):
        """Receive and return the next `AAPMessage`."""
        buf = bytearray()
        msg = None
        while msg is None:
            data = self.socket.recv(1)
            if not data:
                logger.debug("Disconnected")
                return None
            buf += data
            try:
                msg = AAPMessage.parse(buf)
            except InsufficientAAPDataError:
                continue
        return msg

    def send(self, aap_msg):
        """Serialize and send the provided `AAPMessage` to the AAP endpoint.

        Args:
            aap_msg: The `AAPMessage` to be sent.
        """
        self.socket.send(aap_msg.serialize())
        return self.receive()

    def send_bundle(self, dest_eid, bundle_data, bibe=False):
        """Send the provided bundle to the AAP endpoint.

        Args:
            dest_eid: The destination EID.
            bundle_data: The binary payload data to be encapsulated in a
                bundle.
            bibe: Whether the AAPClient should send a SENDBIBE message (True)
                or a SENDBUNDLE message (False). Defaults to False.
        """
        if not bibe:
            logger.debug(f"Sending SENDBUNDLE message to {dest_eid}")
            msg_sendconfirm = self.send(AAPMessage(
                AAPMessageType.SENDBUNDLE, dest_eid, bundle_data
            ))
            assert msg_sendconfirm.msg_type == AAPMessageType.SENDCONFIRM
            try:
                bundle_id = msg_sendconfirm.decode_bundle_id()
            except ValueError:
                bundle_id = msg_sendconfirm.bundle_id
            logger.debug(
                f"SENDCONFIRM message received! ~ ID = {bundle_id}"
            )
            return msg_sendconfirm
        else:
            logger.debug(f"Sending SENDBIBE message to {dest_eid}")
            msg_sendconfirm = self.send(AAPMessage(
                AAPMessageType.SENDBIBE, dest_eid, bundle_data
            ))
            assert msg_sendconfirm.msg_type == AAPMessageType.SENDCONFIRM
            try:
                bundle_id = msg_sendconfirm.decode_bundle_id()
            except ValueError:
                bundle_id = msg_sendconfirm.bundle_id
            logger.debug(
                f"SENDCONFIRM message received! ~ ID = {bundle_id}"
            )
            return msg_sendconfirm

    def send_str(self, dest_eid, bundle_data):
        """Send the provided bundle to the AAP endpoint.

        Args:
            dest_eid: The destination EID.
            bundle_data: The string message to be utf-8 encoded and
                encapsulated in a bundle.
        """
        return self.send_bundle(dest_eid, bundle_data.encode("utf-8"), False)


class AAPUnixClient(AAPClient):
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


class AAPTCPClient(AAPClient):
    """A context manager class for connecting to the AAP TCP socket of a uD3TN
    instance.

    Args:
        address: The address tupel (HOST, PORT) of the remote socket to be used
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
