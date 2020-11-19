#!/usr/bin/env python3
# encoding: utf-8

import logging
import socket
import uuid

from .aap_message import AAPMessage, AAPMessageType, InsufficientAAPDataError


logger = logging.getLogger(__name__)


class AAPClient():
    """A context manager class for connecting to the AAP socket of a uD3TN
    instance.

    Args:
        socket: A `socket.socket` object
        address: The address of the remote socket to be used when calling
            `socket.connect()`
    """

    def __init__(self, socket, address):
        self.socket = socket
        self.address = address
        self.node_eid = None
        self.agent_id = None

    def connect(self):
        """Establish a socket connection to a uD3TN instance and return the
        received welcome message.
        """
        self.socket.connect(self.address)
        logger.info("Connected to uD3TN, awaiting WELCOME message...")
        return self._welcome()

    def disconnect(self):
        """Shutdown and close the socket."""
        logger.info("Terminating connection...")
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
        logger.info(f"WELCOME message received! ~ EID = {msg_welcome.eid}")
        self.node_eid = msg_welcome.eid
        return msg_welcome

    @property
    def eid(self):
        """Return the own EID."""
        return f"{self.node_eid}/{self.agent_id}"

    def register(self, agent_id=None):
        """Attempt to register the specified agent identifier.

        Args:
            agent_id: The agent identifier to be registered. If None,
                uuid.uuid4() is called to generate one.
        """
        self.agent_id = agent_id or str(uuid.uuid4())
        logger.info(f"Sending REGISTER message for '{agent_id}'...")
        msg_ack = self.send(
            AAPMessage(AAPMessageType.REGISTER, self.agent_id)
        )
        assert msg_ack.msg_type == AAPMessageType.ACK
        logger.info("ACK message received!")

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
                logger.info("Disconnected")
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

    def send_bundle(self, dest_eid, bundle_data):
        """Send the provided bundle to the AAP endpoint.

        Args:
            dest_eid: The destination EID.
            bundle_data: The binary payload data to be encapsulated in a
                bundle.
        """
        logger.info(f"Sending SENDBUNDLE message to {dest_eid}")
        msg_sendconfirm = self.send(AAPMessage(
            AAPMessageType.SENDBUNDLE, dest_eid, bundle_data
        ))
        assert msg_sendconfirm.msg_type == AAPMessageType.SENDCONFIRM
        logger.info(
            f"SENDCONFIRM message received! ~ ID = {msg_sendconfirm.bundle_id}"
        )
        return msg_sendconfirm

    def send_str(self, dest_eid, bundle_data):
        """Send the provided bundle to the AAP endpoint.

        Args:
            dest_eid: The destination EID.
            bundle_data: The string message to be utf-8 encoded and
                encapsulated in a bundle.
        """
        return self.send_bundle(dest_eid, bundle_data.encode("utf-8"))


class AAPUnixClient(AAPClient):
    """A context manager class for connecting to the AAP Unix socket of a uD3TN
    instance.

    Args:
        address: The address (PATH) of the remote socket to be used when
            calling `socket.connect()`
    """

    def __init__(self, address='/tmp/ud3tn.socket'):
        super().__init__(
            socket=socket.socket(socket.AF_UNIX, socket.SOCK_STREAM),
            address=address,
        )


class AAPTCPClient(AAPClient):
    """A context manager class for connecting to the AAP TCP socket of a uD3TN
    instance.

    Args:
        address: The address tupel (HOST, PORT) of the remote socket to be used
            when calling `socket.connect()`
    """

    def __init__(self, address=('localhost', 4242)):
        super().__init__(
            socket=socket.socket(socket.AF_INET, socket.SOCK_STREAM),
            address=address,
        )
