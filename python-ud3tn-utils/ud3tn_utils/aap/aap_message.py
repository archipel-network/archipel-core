"""μD3TN Application Agent Protocol (AAP) Python implementation.

Usage:
    Use the AAPMessage class to build, serialize, and parse AAP messages.
    For a description of the protocol, see
    https://gitlab.com/d3tn/ud3tn/-/blob/master/doc/ud3tn_aap.md.
"""

import struct
import enum


class InsufficientAAPDataError(Exception):
    """The binary AAP message appears to be valid but is missing some bytes.

    Args:
        bytes_needed: The total amount of bytes required to perform the next
                      parsing step.
    """

    def __init__(self, bytes_needed):
        self.bytes_needed = bytes_needed


class AAPMessageType(enum.IntEnum):
    """AAP message type codes."""
    ACK = 0x0
    NACK = 0x1
    REGISTER = 0x2
    SENDBUNDLE = 0x3
    RECVBUNDLE = 0x4
    SENDCONFIRM = 0x5
    CANCELBUNDLE = 0x6
    WELCOME = 0x7
    PING = 0x8


class AAPMessage:
    """An AAP message representation supporting parsing and serialization."""
    def __init__(self, msg_type, eid=None, payload=None, bundle_id=None):
        if (msg_type < AAPMessageType.ACK or msg_type > AAPMessageType.PING):
            raise ValueError("Invalid message type code")
        self.msg_type = msg_type
        self.eid = eid
        self.payload = payload
        self.bundle_id = bundle_id

    def serialize(self):
        """Serialize the AAP message to its on-wire representation."""
        msg = [struct.pack("B", 0x10 | (self.msg_type & 0xF))]

        # EID
        if self.msg_type in (AAPMessageType.REGISTER,
                             AAPMessageType.SENDBUNDLE,
                             AAPMessageType.RECVBUNDLE,
                             AAPMessageType.WELCOME):
            msg.append(struct.pack("!H", len(self.eid)))
            msg.append(self.eid.encode("ascii"))

        # Payload
        if self.msg_type in (AAPMessageType.SENDBUNDLE,
                             AAPMessageType.RECVBUNDLE):
            msg.append(struct.pack("!Q", len(self.payload)))
            msg.append(self.payload)

        # Bundle ID
        if self.msg_type in (AAPMessageType.SENDCONFIRM,
                             AAPMessageType.CANCELBUNDLE):
            msg.append(struct.pack("!Q", len(self.bundle_id)))

        return b"".join(msg)

    def __bytes__(self):
        return self.serialize()

    @staticmethod
    def parse(data):
        """Parse the provided AAP on-wire representation.

        An instance of AAPMessage is returned on success.
        If there are not enough bytes provided, an InsufficientAAPDataError is
        raised, allowing the caller to wait for more data.
        If invalid data is provided, a ValueError is raised.
        """
        if len(data) == 0:
            raise InsufficientAAPDataError(1)

        version = (data[0] >> 4) & 0xF
        if version != 1:
            raise ValueError("Invalid AAP version: " + str(version))

        msg_type = data[0] & 0xF

        eid = payload = bundle_id = None
        index = 1

        # EID
        if msg_type in (AAPMessageType.REGISTER,
                        AAPMessageType.SENDBUNDLE,
                        AAPMessageType.RECVBUNDLE,
                        AAPMessageType.WELCOME):
            if len(data) - index < 2:
                raise InsufficientAAPDataError(index + 2)
            eid_length, = struct.unpack("!H", data[index:(index + 2)])
            index += 2
            if len(data) - index < eid_length:
                raise InsufficientAAPDataError(index + eid_length)
            eid = data[index:(index + eid_length)].decode("ascii")
            index += eid_length

        # Payload
        if msg_type in (AAPMessageType.SENDBUNDLE,
                        AAPMessageType.RECVBUNDLE):
            if len(data) - index < 8:
                raise InsufficientAAPDataError(index + 8)
            payload_length, = struct.unpack("!Q", data[index:(index + 8)])
            index += 8
            if len(data) - index < payload_length:
                raise InsufficientAAPDataError(index + payload_length)
            payload = data[index:(index + payload_length)]
            index += payload_length

        # Bundle ID
        if msg_type in (AAPMessageType.SENDCONFIRM,
                        AAPMessageType.CANCELBUNDLE):
            if len(data) - index < 8:
                raise InsufficientAAPDataError(index + 8)
            bundle_id, = struct.unpack("!Q", data[index:(index + 8)])
            index += 8

        return AAPMessage(msg_type, eid, payload, bundle_id)
