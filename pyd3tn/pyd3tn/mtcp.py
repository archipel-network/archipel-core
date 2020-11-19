import socket
import logging

import cbor

from .helpers import sock_recv_raw

logger = logging.getLogger(__name__)


class MTCPSocket(object):
    """Minimal TCP convergence layer socket wrapper."""

    def __init__(self, sock, timeout=None):
        self.sock = sock
        self.timeout = timeout

    def __enter__(self):
        self.connect()
        return self

    def __exit__(self, *args):
        self.disconnect()

    def connect(self):
        """Establish an MTCP connection to a remote peer."""
        # Expect a connected socket by default.
        pass

    def disconnect(self):
        """Close an MTCP connection."""
        logger.debug(
            "MTCP: Terminate connection to %s:%d",
            *self.sock.getpeername(),
        )
        self.sock.shutdown(socket.SHUT_RDWR)
        self.sock.close()

    def send_bundle(self, bundle):
        """Send a single bundle to the remote peer.

        Args:
            bundle (bytes): Serialized bundle
        """
        return self.sock.sendall(cbor.dumps(bundle))

    def recv_bundle(self):
        """Receives a single bundle from the remote peer.

        Returns:
            bundle (bytes): Serialized bundle
        """
        # Receive first byte of CBOR byte string header
        length_type_raw = sock_recv_raw(self.sock, 1, self.timeout)
        additional_information = length_type_raw[0] & ((1 << 5) - 1)
        assert additional_information <= 27
        # Determine the count of following bytes
        if additional_information >= 24:
            additional_bytes = (1 << (additional_information - 24))
            length_type_raw += sock_recv_raw(
                self.sock, additional_bytes, self.timeout
            )
        # Convert CBOR byte string header to uint and decode length
        length_type_cbor_uint = (
            bytes([(length_type_raw[0] & ~0x40)]) +
            length_type_raw[1:]
        )
        byte_string_length = cbor.loads(length_type_cbor_uint)
        # Receive following byte string which is our payload
        data = sock_recv_raw(
            self.sock,
            byte_string_length,
            self.timeout,
        )
        return data


class MTCPConnection(MTCPSocket):
    """Minimal TCP convergence layer connection to a remote DTN node.

    The connection supports the context manager protocol to ensure a proper
    shutdown of the connection in case of any error:

    .. code:: python

        from pyd3tn.mtcp import MTCPConnection

        with MTCPConnection('127.0.0.1', 4224) as conn:
            conn.send(bundle)

    Args:
        host (str): Host to connect to
        port (int): TCP port to connect to
        timeout (float): Timeout for receiving bundles (None for infinite)
    """

    def __init__(self, host, port, timeout=None):
        self.host = host
        self.port = port
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        super().__init__(sock, timeout)

    def connect(self):
        """Establish an MTCP connection to a remote peer."""
        self.sock.connect((self.host, self.port))
        logger.debug("MTCP: Connected to %s:%d", self.host, self.port)
