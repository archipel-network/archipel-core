import enum
import time
import threading

from .helpers import UNIX_TO_DTN_OFFSET
from .sdnv import sdnv_encode


class RFC5050Flag(enum.IntFlag):
    """Default BPv6 flags"""
    IS_FRAGMENT = 0x00001
    ADMINISTRATIVE_RECORD = 0x00002
    MUST_NOT_BE_FRAGMENTED = 0x00004
    CUSTODY_TRANSFER_REQUESTED = 0x00008
    SINGLETON_ENDPOINT = 0x00010
    ACKNOWLEDGEMENT_REQUESTED = 0x00020

    # Priority
    NORMAL_PRIORITY = 0x00080
    EXPEDITED_PRIORITY = 0x00100

    # Reports
    REPORT_RECEPTION = 0x04000
    REPORT_CUSTODY_ACCEPTANCE = 0x08000
    REPORT_FORWARDING = 0x10000
    REPORT_DELIVERY = 0x20000
    REPORT_DELETION = 0x40000


RFC5050Flag.DEFAULT_OUTGOING = (
    RFC5050Flag.SINGLETON_ENDPOINT |
    RFC5050Flag.NORMAL_PRIORITY
)


class BlockType(enum.IntEnum):
    """(Extension) block types"""
    PAYLOAD = 1
    PREVIOUS_NODE = 7
    BUNDLE_AGE = 8
    HOP_COUNT = 9
    MAX = 255


class BlockFlag(enum.IntFlag):
    """(Extension) block flags"""
    NONE = 0x00
    MUST_BE_REPLICATED = 0x01
    REPORT_IF_UNPROC = 0x02
    DELETE_BUNDLE_IF_UNPROC = 0x04
    LAST_BLOCK = 0x08
    DISCARD_IF_UNPROC = 0x10
    FWD_UNPROC = 0x20
    HAS_EID_REF_FIELD = 0x40


NULL_EID = "dtn:none"


_th_local = threading.local()


def next_sequence_number():
    seqnum = _th_local.__dict__.get("seqnum", 0)
    _th_local.__dict__["seqnum"] = seqnum + 1
    return seqnum


def reset_sequence_number():
    _th_local.__dict__["seqnum"] = 0


def serialize_bundle6(source_eid, destination_eid, payload,
                      report_to_eid=NULL_EID, custodian_eid=NULL_EID,
                      creation_timestamp=None, sequence_number=None,
                      lifetime=300, flags=RFC5050Flag.DEFAULT_OUTGOING,
                      fragment_offset=None, total_adu_length=None):
    # RFC 5050 header: https://tools.ietf.org/html/rfc5050#section-4.5
    # Build part before "primary block length"
    header_part1 = bytearray()
    header_part1.append(0x06)
    header_part1 += sdnv_encode(flags)

    # Build part after "primary block length"
    header_part2 = bytearray()

    # NOTE: This does not do deduplication (which is optional) currently.
    dictionary = bytearray()
    cur_dict_offset = 0
    for eid in (destination_eid, source_eid, report_to_eid, custodian_eid):
        scheme, ssp = eid.encode("ascii").split(b":", 1)
        dictionary += scheme + b"\0"
        header_part2 += sdnv_encode(cur_dict_offset)
        cur_dict_offset += len(scheme) + 1
        dictionary += ssp + b"\0"
        header_part2 += sdnv_encode(cur_dict_offset)
        cur_dict_offset += len(ssp) + 1

    if creation_timestamp is None:
        creation_timestamp = int(time.time() - UNIX_TO_DTN_OFFSET)
    header_part2 += sdnv_encode(creation_timestamp)

    # If the sequence number is None, the last thread-local sequence number
    # will be used, incremented by one.
    if sequence_number is None:
        sequence_number = next_sequence_number()
    header_part2 += sdnv_encode(sequence_number)

    header_part2 += sdnv_encode(lifetime)

    header_part2 += sdnv_encode(len(dictionary))
    header_part2 += dictionary

    if fragment_offset is not None and total_adu_length is not None:
        assert (flags & RFC5050Flag.IS_FRAGMENT) != 0
        header_part2 += sdnv_encode(fragment_offset)
        header_part2 += sdnv_encode(total_adu_length)

    # Add the length of all remaining fields as primary block length
    header = header_part1 + sdnv_encode(len(header_part2)) + header_part2

    # Build payload block
    # https://tools.ietf.org/html/rfc5050#section-4.5.2
    payload_block = bytearray()
    payload_block.append(BlockType.PAYLOAD)
    payload_block += sdnv_encode(BlockFlag.LAST_BLOCK)
    # Block length is the length of the remaining part of the block (PL data)
    payload_block += sdnv_encode(len(payload))
    payload_block += payload

    # NOTE: This does _not_ support extension blocks currently
    return bytes(header + payload_block)
