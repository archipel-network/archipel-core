import enum
import time
import struct

from collections import namedtuple
from datetime import datetime, timezone

UNIX_EPOCH = datetime(1970, 1, 1, tzinfo=timezone.utc)
DTN_EPOCH = datetime(2000, 1, 1, tzinfo=timezone.utc)
UNIX_TO_DTN_OFFSET = (DTN_EPOCH - UNIX_EPOCH).total_seconds()
assert UNIX_TO_DTN_OFFSET == 946684800


def unix2dtn(unix_timestamp):
    """Converts a given Unix timestamp into a DTN timestamp

    Its inversion is :func:`dtn2unix`.

    Args:
        unix_timestamp: Unix timestamp
    Returns:
        numeric: DTN timestamp
    """
    return unix_timestamp - UNIX_TO_DTN_OFFSET


class RouterCommand(enum.IntEnum):
    """uD3TN Command Constants"""
    ADD = 1
    UPDATE = 2
    DELETE = 3
    QUERY = 4


Contact = namedtuple('Contact', ['start', 'end', 'bitrate'])
Contact.__doc__ = """named tuple holding uD3TN contact information

Attrs:
    start (int): DTN timestamp when the contact starts
    end (int): DTN timestamp when the contact is over
    bitrate (int): Bitrate of the contact
"""


def make_contact(start_offset, duration, bitrate):
    """Create a :class:`Contact` tuple relative to the current time

    Args:
        start_offset (int): Start point of the contact from in seconds from now
        duration (int): Duration of the contact in seconds
        bitrate (int): Bitrate of the contact
    Returns:
        Contact: contact tuple with DTN timestamps
    """
    cur_time = time.time()
    start = unix2dtn(cur_time + start_offset)

    return Contact(
        start=int(round(start)),
        end=int(round(start + duration)),
        bitrate=int(round(bitrate)),
    )


class ConfigMessage(object):
    """uD3TN configuration message that can be processes by its config agent.
    These messages are used to configure contacts in uD3TN.

    Args:
        eid (str): The endpoint identifier of a contact
        cla_address (str): The Convergency Layer Adapter (CLA) address for the
            contact's EID
        reachable_eids (List[str], optional): List of reachable EIDs via this
            contact
        contacts (List[Contact], optional): List of contacts with the node
        type (RouterCommand, optional): Type of the configuration message (add,
            remove, ...)
    """

    def __init__(self, eid, cla_address, reachable_eids=None, contacts=None,
                 type=RouterCommand.ADD):
        self.eid = eid
        self.cla_address = cla_address
        self.reachable_eids = reachable_eids or []
        self.contacts = contacts or []
        self.type = type

    def __repr__(self):
        return "<ConfigMessage {!r} {} reachable={} contacts={}>".format(
            self.eid, self.cla_address, self.reachable_eids, self.contacts
        )

    def __str__(self):
        # missing escaping has to be addresses in uD3TN
        for part in [self.eid, self.cla_address] + self.reachable_eids:
            assert "(" not in part
            assert ")" not in part

        if self.reachable_eids:
            eid_list = "[" + ",".join(
                "(" + eid + ")" for eid in self.reachable_eids
            ) + "]"
        else:
            eid_list = ""

        if self.contacts:
            contact_list = (
                    "[" +
                    ",".join(
                        "{{{},{},{}}}".format(start, end, bitrate)
                        for start, end, bitrate in self.contacts
                    ) +
                    "]"
                )
        else:
            contact_list = ""

        return "{}({}):({}):{}:{};".format(
            self.type,
            self.eid,
            self.cla_address,
            eid_list,
            contact_list,
        )

    def __bytes__(self):
        return str(self).encode('ascii')


class ManagementCommand(enum.IntEnum):
    """uD3TN Management Command Constants"""
    SET_TIME = 0


def serialize_set_time_cmd(unix_timestamp):
    dtn_timestamp = int(unix2dtn(unix_timestamp))
    binary = [
        # Header
        struct.pack('B', int(ManagementCommand.SET_TIME)),
        struct.pack('!Q', dtn_timestamp),
    ]
    return b"".join(binary)
