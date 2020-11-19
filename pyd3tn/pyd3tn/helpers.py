import select
from time import time
from datetime import datetime, timezone

UNIX_EPOCH = datetime(1970, 1, 1, tzinfo=timezone.utc)
DTN_EPOCH = datetime(2000, 1, 1, tzinfo=timezone.utc)
CCSDS_EPOCH = datetime(1958, 1, 1, tzinfo=timezone.utc)

UNIX_TO_DTN_OFFSET = (DTN_EPOCH - UNIX_EPOCH).total_seconds()
assert UNIX_TO_DTN_OFFSET == 946684800

CCSDS_TO_UNIX_OFFSET = (UNIX_EPOCH - CCSDS_EPOCH).total_seconds()
CCSDS_TO_DTN_OFFSET = (DTN_EPOCH - CCSDS_EPOCH).total_seconds()


def unix2dtn(unix_timestamp):
    """Converts a given Unix timestamp into a DTN timestamp

    Its inversion is :func:`dtn2unix`.

    Args:
        unix_timestamp: Unix timestamp
    Returns:
        numeric: DTN timestamp
    """
    return unix_timestamp - UNIX_TO_DTN_OFFSET


def dtn2unix(dtn_timestamp):
    """Converts a given DTN timestamp into a Unix timestamp

    Its inversion is :func:`unix2dtn`.

    Args:
        dtn_timestamp: DTN timestamp
    Returns:
        numeric: Unix timestamp
    """
    return dtn_timestamp + UNIX_TO_DTN_OFFSET


def dtntime():
    """Obtains the current DTN timestamp.

    Returns:
        float: DTN timestamp
    """
    return unix2dtn(time())


def ccsdstime():
    """Obtains the current CCSDS timestamp.

    Returns:
        float: CCSDS timestamp
    """
    return time() + CCSDS_TO_UNIX_OFFSET


def sock_recv_raw(sock, count, timeout=None):
    """Tries to receive an exact number of bytes from a socket.

    Args:
        sock: socket object
        count (int): number of bytes to be received
        timeout (float): maximum time to wait, in s (None for infinite timeout)
    Returns:
        bytes: Received raw data
    """
    assert count
    buf = b""
    while len(buf) < count:
        if timeout:
            ready = select.select([sock], [], [], timeout)
        else:
            ready = select.select([sock], [], [])
        assert ready[0], "select operation ran into timeout"
        r = sock.recv(count - len(buf))
        assert len(r), "received 0 bytes (e.g. because the socket was closed)"
        buf += r
    return buf
