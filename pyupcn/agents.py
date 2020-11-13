import enum
import time
import struct

from collections import namedtuple
from .helpers import unix2dtn


class RouterCommand(enum.IntEnum):
    """uPCN Command Constants"""
    ADD    = 1
    UPDATE = 2
    DELETE = 3
    QUERY  = 4


Contact = namedtuple('Contact', ['start', 'end', 'bitrate'])
Contact.__doc__ = """named tuple holding upcn contact information

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
    """upcn configuration message that can be processes by its config agent.
    These messages are used to configure contacts in upcn.

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
        # missing escaping has to be addresses in uPCN
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
    """uPCN Management Command Constants"""
    SET_TIME = 0


def serialize_set_time_cmd(unix_timestamp):
    dtn_timestamp = int(unix2dtn(unix_timestamp))
    binary = [
        # Header
        struct.pack('B', int(ManagementCommand.SET_TIME)),
        struct.pack('!Q', dtn_timestamp),
    ]
    return b"".join(binary)
