import enum
import struct
import socket
import logging
import asyncio

from .sdnv import sdnv_encode, sdnv_decode
from .helpers import sock_recv_raw


logger = logging.getLogger(__name__)


class TCPCLConnectionFlag(enum.IntFlag):
    NONE = 0x00
    REQUEST_ACK = 0x01
    REACTIVE_FRAGMENTATION = 0x02
    ALLOW_REFUSAL = 0x04
    REQUEST_LENGTH = 0x08
    DEFAULT = NONE


def serialize_tcpcl_contact_header(source_eid,
                                   flags=TCPCLConnectionFlag.DEFAULT,
                                   keepalive_interval=0):
    # https://tools.ietf.org/html/rfc7242#section-4.1
    result = bytearray()
    result += b"dtn!"  # TCPCL header magic
    result += b"\x03"  # version = v3
    result.append(flags)
    result += struct.pack("!H", keepalive_interval)
    source_eid_bin = source_eid.encode("ascii")
    result += sdnv_encode(len(source_eid_bin))
    result += source_eid_bin
    return result


def decode_tcpcl_contact_header(header):
    assert header[0:4] == b"dtn!", "header magic not found"
    assert header[4] == 0x03, "only TCPCL v3 is supported"
    assert len(header) >= 9, "corrupt (too short) TCPCL header"
    result = {
        "version": header[4],
        "flags": header[5],
        "keepalive_interval": struct.unpack('!H', header[6:8])[0],
    }
    eid_len, end_offset = sdnv_decode(header[8:])
    assert len(header) == 8 + end_offset + eid_len, "wrong header length"
    result["eid"] = header[8 + end_offset:].decode("ascii")
    return result


def serialize_tcpcl_single_bundle_segment(bundle_data):
    # https://tools.ietf.org/html/rfc7242#section-5.2
    result = bytearray()
    message_type = 0x1  # DATA_SEGMENT
    message_flags = 0x3  # S | E = first AND last segment of bundle
    result.append((message_type << 4) | message_flags)
    result += sdnv_encode(len(bundle_data))
    result += bundle_data
    return result


def decode_tcpcl_bundle_segment_header(bindata):
    message_type = (bindata[0] & 0xF0) >> 4
    message_flags = bindata[0] & 0x0F
    assert message_type == 0x1, "only DATA_SEGMENT messages are supported"
    assert message_flags == 0x3, "only unsegmented bundles are supported"
    length, _ = sdnv_decode(bindata[1:])
    return message_type, message_flags, length


class TCPCLConnection(object):
    """TCPCL connection to a remote DTN node

    The connection supports the context manager protocol to ensure a proper
    shutdown of the connection in case of any error:

    .. code:: python

        from pyd3tn.tcpcl import TCPCLConnection

        with TCPCLConnection('dtn://my-eid.dtn', '127.0.0.1', 4223) as conn:
            conn.send(bundle)

    Args:
        eid (str): Endpoint identifier for this DTN node
        host (str): Host to connect to
        port (int): TCP port to connect to
        timeout (float): Timeout for receiving bundles (None for infinite)
    """
    def __init__(self, eid, host, port, timeout=None):
        self.eid = eid
        self.host = host
        self.port = port
        self.timeout = timeout
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    def __enter__(self):
        self.connect()
        return self

    def __exit__(self, *args):
        self.disconnect()

    def connect(self):
        """Establish a TCPCL connection to a remote peer.

        Returns:
            dict: Deserialized TCPCL contact header
        """
        if not self.sock:
            return

        # Establish TCP connection
        self.sock.connect((self.host, self.port))
        self.sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        logger.debug("TCPCL: Connected to %s:%d", self.host, self.port)

        # Send TCPCL contact header with own EID
        logger.debug("TCPCL: Send header for %r", self.eid)
        self.sock.sendall(serialize_tcpcl_contact_header(self.eid))

        # Receive TCPCL contact header
        header = decode_tcpcl_contact_header(self.sock.recv(1024))
        logger.debug("TCPCL: Received header: %r", header)

        return header

    def disconnect(self):
        # Terminate TCPCL connection
        logger.debug(
            "TCPCL: Terminate connection to %s:%d",
            *self.sock.getpeername()
        )
        self.sock.sendall(b"\x50")
        self.sock.shutdown(socket.SHUT_RDWR)
        self.sock.close()

    def send_bundle(self, bundle):
        """Sends a single bundle to the remote peer.

        Args:
            bundle (bytes): Serialized bundle
        """
        return self.sock.sendall(serialize_tcpcl_single_bundle_segment(bundle))

    def recv_bundle(self):
        """Receives a single bundle from the remote peer.

        Returns:
            bundle (bytes): Serialized bundle
        """
        hdr_raw = sock_recv_raw(self.sock, 2, self.timeout)
        c = 0
        # Incrementally read length until end of SDNV is detected (limit = 10)
        while (hdr_raw[-1] & 0x80) != 0 and c < 10:
            hdr_raw += sock_recv_raw(self.sock, 1, self.timeout)
            c += 1
        assert (hdr_raw[-1] & 0x80) == 0, (
            "invalid header or SDNV longer than 10 bytes"
        )
        type_, flags, length = decode_tcpcl_bundle_segment_header(hdr_raw)
        data = sock_recv_raw(
            self.sock,
            length,
            self.timeout,
        )
        return data


class AsyncTCPCLConnection(TCPCLConnection):
    """Asyncio-variant of the TCPCL connection

    It supports the async context manager protocol:

    .. code:: python

        async with AsyncTCPCLConnection(me) as conn:
            conn.bind('127.0.0.1', 42421)
            await conn.connect()
            await conn.send(bundle)

    Args:
        eid (str): Endpoint identifier for this DTN node
        loop (asyncio.AbstractEventLoop, optional): Event loop that should be
            used. If not given the default :func:`asyncio.get_event_loop` will
            be used.
    """

    def __init__(self, eid, loop=None):
        super().__init__(eid)

        self.loop = loop or asyncio.get_event_loop()
        self.reader = None
        self.writer = None

    async def __aenter__(self):
        self.__enter__()
        self.sock.setblocking(False)
        return self

    async def connect(self, host='127.0.0.1', port=4556):
        """Async variant of :meth:`TCPCLConnection.connect`. The interface is
        identical, except that this function is a coroutine and has the be
        awaited.
        """
        if not self.sock:
            return

        # Establish TCP connection
        await self.loop.sock_connect(
            self.sock, (host, port)
        )
        # Create stream reader and writer pair
        self.reader, self.writer = await asyncio.open_connection(
            sock=self.sock, loop=self.loop
        )
        logger.debug("TCPCL: Connected to %s:%d", host, port)

        # Send TCPCL contact header with own EID
        logger.debug("TCPCL: Send header for %r", self.eid)
        self.writer.write(serialize_tcpcl_contact_header(self.eid))
        await self.writer.drain()

        # Receive TCPCL contact header
        raw = await self.reader.read(1024)
        header = decode_tcpcl_contact_header(raw)
        logger.debug("TCPCL: Received header: %r", header)
        return header

    async def send(self, bundle):
        """Async variant of :meth:`TCPCLConnection.send`. The interface is
        identical, except that this function is a coroutine and has the be
        awaited.
        """
        if not self.sock:
            return

        self.writer.write(serialize_tcpcl_single_bundle_segment(bundle))
        return await self.writer.drain()

    async def __aexit__(self, *args):
        # Terminate TCPCL connection if established
        if self.writer:
            logger.debug("TCPCL: Terminate connection to %s:%d",
                         *self.writer.transport.get_extra_info('peername'))
            self.writer.write(b"\x50")
            await self.writer.drain()

            # Close TCP connection
            self.writer.close()

        self.writer = None
        self.reader = None
        self.sock = False


class TCPCLServer(object):
    """Simple asyncio-based TCPCL server

    By default this server does nothing with received messages. It is a simple
    data sink. Messages are handled by the :meth:`message_received` method
    which does nothing in the default implementation. Subclasses can overwrite
    this method to perform specific tasks on recevied TCPCL messages.

    This class can be used as async context manager to ensure the connections
    are closed correctly even in case of an error:

    .. code:: python

        import asyncio
        from pyd3tn.tcpcl import TCPCLServer

        async def listen():
            async with TCPCLServer('dtn:me', '127.0.0.1', 42420) as server:
                # do some other cool stuff here while the server is running in
                # the background ... or just wait for the server to finish
                await server.wait_closed()

        loop = asyncio.get_event_loop()
        loop.run_until_complete(listen())

    Args:
        eid (str): The endpoint identifier (EID) that is used for this DTN node
        host (str): Hostname / IP address the server should listening on
        port (int): TCP port number the server should listenin on
        backlog (int, optional): Number of concurrent TCP connections
        loop (asyncio.AbstractEventLoop, optional): Event loop that should be
            used. If not given the default :func:`asyncio.get_event_loop` will
            be used.
    """

    def __init__(self, eid, host, port, backlog=100, loop=None):
        self.eid = eid
        self.host = host
        self.port = port
        self.backlog = backlog
        self.server = None
        self.handlers = []
        self.loop = loop or asyncio.get_event_loop()

    async def start(self):
        """Start listening on the specified host and port"""
        if self.server:
            return

        self.server = await asyncio.start_server(
            self.client_connected, host=self.host, port=self.port,
            backlog=self.backlog, loop=self.loop
        )
        logger.debug("TCPCLServer: Listening on %s:%d", self.host, self.port)

    def close(self):
        """Terminate all open connections and close the listening socket"""
        if not self.server:
            return

        logger.debug("TCPCLServer: Closing")

        # Terminate all active handler tasks
        for task in self.handlers:
            task.cancel()

        # Close all connections and wait from them to be closed
        self.server.close()

    async def wait_closed(self):
        if not self.server:
            return

        await self.server.wait_closed()
        self.server = None

    async def client_connected(self, reader, writer):
        """Callback function executed whenever a new TCP connection is
        established.

        It creates a handler task for each new connection. Handler tasks are
        stored in the :attr:`handlers` list.

        Args:
            reader (asyncio.StreamReader): Reader for the underlying TCP
                connection
            writer (asyncio.StreamWriter): Writer for the underlying TCP
                connection
        """
        # We create an extra task for handling the connection because we want
        # to be able to cancel them. A call to "self.server.close()" would stop
        # this coroutine without any exception. Hence we have no possibility to
        # shutdown the TCPCL connection properly in this coroutine.
        task = self.loop.create_task(self.handle_connection(reader, writer))
        task.add_done_callback(lambda task: self.handlers.remove(task))
        self.handlers.append(task)

    async def handle_connection(self, reader, writer):
        """This method is executed for each new connection is a separate
        asyncio task. It sends and receives the TCPCL contact header and then
        receives messages and forwards them to the :meth:`received_message`
        method. In case of any error the TCPCL connection is terminated
        gracefully by sending `\x50` to the remote peer.

        Args:
            reader (asyncio.StreamReader): Reader for the underlying TCP
                connection
            writer (asyncio.StreamWriter): Writer for the underlying TCP
                connection
        """
        logger.debug("TCPCLServer: Connection from %s:%d",
                     *writer.transport.get_extra_info('peername'))
        try:
            writer.write(serialize_tcpcl_contact_header(self.eid))
            await writer.drain()

            # TCPCL header
            raw = await reader.read(1024)
            if not raw:
                logger.debug("TCPCLServer: Remote peer closed connection")
                return
            decode_tcpcl_contact_header(raw)

            # TCPCL messages
            while True:
                msg = await reader.read(1024)
                if not msg:
                    logger.debug("TCPCLServer: Remote peer closed connection")
                    return
                await self.received_message(msg, writer)
        except asyncio.CancelledError:
            pass
        # In case of any other exception, log the error and terminate the
        # connection
        except Exception as err:
            logger.exception(err)
        finally:
            logger.debug("TCPCLServer: Close connection from %s:%d",
                         *writer.transport.get_extra_info('peername'))

            # Gracefully terminate TCPCL session if TCP connection is not
            # already closing.
            if not writer.transport.is_closing():
                writer.write(b"\x50")
                await writer.drain()
                writer.close()

    async def received_message(self, msg, writer):
        """Message handler called whenever a message gets received. It does
        nothing but logging the message by default. Subclasses can overwrite
        this method to perform specific tasks.

        Args:
            msg (bytes): TCPCL message received
            writer (asyncio.StreamWriter): Writer for the TCP connection that
                can be used to react on the received message
        """
        logger.info("TCPCLServer: Received: %s", msg.hex())

    async def __aenter__(self):
        await self.start()
        return self

    async def __aexit__(self, *args):
        self.close()
        await self.wait_closed()
