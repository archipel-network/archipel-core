"""BPbis bundle Generator

A simple Python package for generating and serialize BPbis bundles into CBOR.

Dependencies:
    This module depends on the ``cbor`` package which can be installed via pip:

    .. code:: bash

        pip install cbor
"""
import struct
import threading
from enum import IntEnum, IntFlag
from datetime import datetime, timezone, timedelta
from binascii import hexlify
import cbor  # type: ignore
from cbor.cbor import CBOR_ARRAY  # type: ignore
from .crc import crc16_x25, crc32_c
from .helpers import DTN_EPOCH, UNIX_TO_DTN_OFFSET


__all__ = [
    'BundleProcFlag',
    'CRCType',
    'BlockType',
    'BlockProcFlag',
    'RecordType',
    'ReasonCode',
    'StatusCode',
    'EID',
    'CreationTimestamp',
    'PrimaryBlock',
    'CanonicalBlock',
    'PayloadBlock',
    'AdministrativeRecord',
    'BundleStatusReport',
    'PreviousNodeBlock',
    'BundleAgeBlock',
    'HopCountBlock',
    'Bundle',
    'serialize_bundle7',
]


class BundleProcFlag(IntFlag):
    NONE = 0x000000
    IS_FRAGMENT = 0x000001
    ADMINISTRATIVE_RECORD = 0x000002
    MUST_NOT_BE_FRAGMENTED = 0x000004
    ACKNOWLEDGEMENT_REQUESTED = 0x000020
    REPORT_STATUS_TIME = 0x000040
    REPORT_RECEPTION = 0x004000
    REPORT_FORWARDING = 0x010000
    REPORT_DELIVERY = 0x020000
    REPORT_DELETION = 0x400000


class CRCType(IntEnum):
    NONE = 0   # indicates "no CRC is present."
    CRC16 = 1  # indicates "a CRC-16 (a.k.a., CRC-16-ANSI) is present."
    CRC32 = 2  # indicates "a standard IEEE 802.3 CRC-32 is present."


class BlockType(IntEnum):
    PRIMARY = -1
    PAYLOAD = 1
    PREVIOUS_NODE = 6
    BUNDLE_AGE = 7
    HOP_COUNT = 10


class BlockProcFlag(IntFlag):
    NONE = 0x00
    MUST_BE_REPLICATED = 0x01
    DISCARD_IF_UNPROC = 0x02
    REPORT_IF_UNPROC = 0x04
    DELETE_BUNDLE_IF_UNPROC = 0x10


class RecordType(IntEnum):
    BUNDLE_STATUS_REPORT = 1


class ReasonCode(IntEnum):
    """Bundle status report reason codes"""
    NO_INFO = 0
    LIFETIME_EXPIRDE = 1
    FORWARDED_UNIDIRECTIONAL = 2
    TRANSMISSION_CANCELED = 3
    DEPLETED_STORAGE = 4
    DEST_EID_UNINTELLIGIBLE = 5
    NO_KNOWN_ROUTE = 6
    NO_TIMELY_CONTACT = 7
    BLOCK_UNINTELLIGIBLE = 8
    HOP_LIMIT_EXCEEDED = 9
    TRAFFIC_PARED = 10


class StatusCode(IntEnum):
    RECEIVED_BUNDLE = 0x01
    FORWARDED_BUNDLE = 0x04
    DELIVERED_BUNDLE = 0x08
    DELETED_BUNDLE = 0x10


class EID(tuple):
    """BPbis Endpoint Identifier"""
    def __new__(cls, eid):
        # Tuples are immutable. Hence the construction logic is in the __new__
        # method instead in __init__.
        if eid is None or eid == 'dtn:none':
            return super().__new__(cls, (1, 0))

        # Copy existing EID
        if isinstance(eid, EID) or isinstance(eid, tuple):
            return super().__new__(cls, (eid[0], eid[1]))

        try:
            schema, ssp = eid.split(":")

            if schema == 'dtn':
                return super().__new__(cls, (1, ssp))
            elif schema == 'ipn':
                nodenum, servicenum = ssp.split('.')
                return super().__new__(
                    cls, (2, (int(nodenum), int(servicenum)))
                )
        except ValueError:
            raise ValueError("Invalid EID {!r}".format(eid))

        raise ValueError("Unknown schema {!r}".format(schema))

    @property
    def schema(self):
        """Schema (``dtn`` or ``ipn``) of the EID. This is encoded in the first
        tuple element."""
        if self[0] == 1:
            return 'dtn'
        elif self[0] == 2:
            return 'ipn'
        else:
            raise ValueError("Unknown schema {!r}".format(self[0]))

    @property
    def ssp(self):
        """Scheme-specific part (SSP) of the EID. This is encoded in the second
        tuple element.
        """
        if self[0] == 2:
            return "{}.{}".format(*self[1])
        else:
            if self[1] == 0:
                return "none"
            else:
                return self[1]

    def __str__(self):
        return "{}:{}".format(self.schema, self.ssp)

    def __repr__(self):
        # Reuse the __str__ method here
        return "<EID '{}'>".format(self)

    @staticmethod
    def from_cbor(cbor_data):
        assert len(cbor_data) == 2
        return EID((cbor_data[0], cbor_data[1]))


class CreationTimestamp(tuple):
    """BPbis creation timestamp tuple consisting of

        (<DTN timestamp>, <sequence number>)

    Args:
        time (None, int, datetime.datetime): Timestamp given as Unix timestamp
            (integer) or a Python timezone-aware datetime object. If the time
            is None, the current time will be used. If the time is 0, no
            conversion is performed and the DTN timestamp is set to 0.
        sequence_number (int): Sequence number of the bundle if the device is
            lacking a precise clock
    """
    def __new__(cls, time, sequence_number):
        if time == 0:
            return super().__new__(cls, [0, int(sequence_number)])

        # Use current datetime
        if time is None:
            time = datetime.now(timezone.utc)
        # Convert Unix timestamp into UTC datetime object
        elif isinstance(time, int) or isinstance(time, float):
            time = datetime.fromtimestamp(time, timezone.utc)

        return super().__new__(cls, [
            int(round((time - DTN_EPOCH).total_seconds() * 1000)),
            int(sequence_number),
        ])

    @property
    def time(self):
        """Returns the DTN timestamp value of the CreationTimestamp tuple as
        datetime object or None if it's 0.
        """
        return (
            None if self[0] == 0
            else DTN_EPOCH + timedelta(milliseconds=self[0])
        )

    @property
    def sequence_number(self):
        return self[1]

    def __repr__(self):
        return "<CreationTimestamp time={} sequence={}>".format(
                    self.time, self.sequence_number)

    @staticmethod
    def from_cbor(cbor_data):
        assert len(cbor_data) == 2
        return CreationTimestamp(
            cbor_data[0] / 1000 + UNIX_TO_DTN_OFFSET, cbor_data[1]
        )


class PrimaryBlock(object):
    """The primary bundle block contains the basic information needed to
    forward bundles to their destination.
    """
    def __init__(self, bundle_proc_flags=BundleProcFlag.NONE,
                 crc_type=CRCType.CRC16,
                 destination=None,
                 source=None,
                 report_to=None,
                 creation_time=None,
                 lifetime=24 * 60 * 60 * 1000,
                 fragment_offset=None,
                 total_payload_length=None,
                 crc_provided=None):
        self.version = 7
        self.bundle_proc_flags = bundle_proc_flags
        self.crc_type = crc_type
        self.destination = EID(destination)
        self.source = EID(source)
        self.report_to = EID(report_to)

        if not creation_time:
            creation_time = CreationTimestamp(datetime.now(timezone.utc), 0)

        self.creation_time = creation_time
        self.lifetime = lifetime

        # Optional fields
        self.fragment_offset = fragment_offset
        self.total_payload_length = total_payload_length

        self.crc_provided = crc_provided

    def has_flag(self, required):
        return self.bundle_proc_flags & required == required

    @property
    def block_number(self):
        return 0

    @property
    def block_type(self):
        return BlockType.PRIMARY

    def as_array(self):
        primary_block = [
            self.version,
            self.bundle_proc_flags,
            self.crc_type,
            self.destination,
            self.source,
            self.report_to,
            self.creation_time,
            self.lifetime,
        ]

        # Fragmentation
        if self.has_flag(BundleProcFlag.IS_FRAGMENT):

            assert self.fragment_offset is not None, (
                "Fragment offset must be present for fragmented bundles"
            )
            assert self.total_payload_length is not None, (
                "Total payload length must be present for fragmented bundles"
            )

            primary_block.append(self.fragment_offset)
            primary_block.append(self.total_payload_length)

        return primary_block

    def calculate_crc(self):
        primary_block = self.as_array()
        assert self.crc_type != CRCType.NONE
        binary = [
            # CBOR Array header
            struct.pack('B', 0x80 | len(primary_block) + 1),
        ]

        # encode each field
        for field in primary_block:
            binary.append(cbor.dumps(field))

        if self.crc_type == CRCType.CRC32:
            # Empty CRC-32 bit field: CBOR "byte string"
            binary.append(b'\x44\x00\x00\x00\x00')
            crc = crc32_c(b''.join(binary))
        else:
            binary.append(b'\x42\x00\x00')
            crc = crc16_x25(b''.join(binary))

        return crc

    def __bytes__(self):
        primary_block = self.as_array()

        if self.crc_type != CRCType.NONE:
            crc = self.calculate_crc()
            if self.crc_type == CRCType.CRC32:
                primary_block.append(struct.pack('!I', crc))
            else:
                primary_block.append(struct.pack('!H', crc))

        return cbor.dumps(primary_block)

    def __repr__(self):
        return (
            "PrimaryBlock(bundle_proc_flags={}, crc_type={}, "
            "destination={}, source={}, report_to={}, "
            "creation_time={}, lifetime={}, fragment_offset={}, "
            "total_payload_length={}, crc_provided={})"
        ).format(
            repr(self.bundle_proc_flags),
            repr(self.crc_type),
            repr(self.destination),
            repr(self.source),
            repr(self.report_to),
            self.creation_time,
            self.lifetime,
            self.fragment_offset,
            self.total_payload_length,
            (
                "0x{:08x}".format(self.crc_provided)
                if self.crc_provided is not None else "None"
            ),
        )

    @staticmethod
    def from_cbor(cbor_data):
        version = cbor_data[0]
        assert version == 7, "This class can only decode BPv7 bundles."
        bundle_proc_flags = BundleProcFlag(cbor_data[1])
        crc_type = CRCType(cbor_data[2])

        expected_fields = 8

        fragment_offset = None
        total_payload_length = None
        if (bundle_proc_flags & BundleProcFlag.IS_FRAGMENT) != 0:
            expected_fields += 2
            fragment_offset = cbor_data[8]
            total_payload_length = cbor_data[9]

        crc = None
        if crc_type != CRCType.NONE:
            crc_data = cbor_data[expected_fields]  # obtain last field of array
            expected_fields += 1
            if crc_type == CRCType.CRC32:
                crc = struct.unpack('!I', crc_data)[0]
            else:
                crc = struct.unpack('!H', crc_data)[0]

        return PrimaryBlock(
            bundle_proc_flags=bundle_proc_flags,
            crc_type=crc_type,
            destination=EID.from_cbor(cbor_data[3]),
            source=EID.from_cbor(cbor_data[4]),
            report_to=EID.from_cbor(cbor_data[5]),
            creation_time=CreationTimestamp.from_cbor(cbor_data[6]),
            fragment_offset=fragment_offset,
            total_payload_length=total_payload_length,
            lifetime=(cbor_data[7]),
            crc_provided=crc,
        )


class CanonicalBlock(object):
    """Canonical bundle block structure"""
    def __init__(self, block_type, data,
                 block_number=None,
                 block_proc_flags=BlockProcFlag.NONE,
                 crc_type=CRCType.CRC32,
                 crc_provided=None):
        self.block_type = block_type
        self.block_proc_flags = block_proc_flags
        self.block_number = block_number
        self.crc_type = crc_type
        self.crc_provided = crc_provided
        self.data = data

    def as_array(self):
        return [
            self.block_type,
            self.block_number,
            self.block_proc_flags,
            self.crc_type,
            self.data,
        ]

    def calculate_crc(self):
        assert self.crc_type != CRCType.NONE
        block = self.as_array()
        if self.crc_type == CRCType.CRC32:
            # Empty CRC-32 bit field: CBOR "byte string"
            empty_crc = b'\x44\x00\x00\x00\x00'
        else:
            empty_crc = b'\x42\x00\x00'

        binary = struct.pack('B', 0x80 | (len(block) + 1)) + b''.join(
            [cbor.dumps(item) for item in block]
        ) + empty_crc

        if self.crc_type == CRCType.CRC32:
            crc = crc32_c(binary)
            # unsigned long in network byte order
            block.append(struct.pack('!I', crc))
        else:
            crc = crc16_x25(binary)
            # unsigned short in network byte order
            block.append(struct.pack('!H', crc))
        return crc

    def __bytes__(self):
        assert isinstance(self.data, bytes)
        block = self.as_array()

        if self.crc_type != CRCType.NONE:
            crc = self.calculate_crc()
            if self.crc_type == CRCType.CRC32:
                block.append(struct.pack('!I', crc))
            else:
                block.append(struct.pack('!H', crc))

        return cbor.dumps(block)

    def __repr__(self):
        return (
            "{}(block_type={}, block_number={}, block_proc_flags={}, "
            "data_length={}, crc_type={}, crc_provided={})"
        ).format(
            self.__class__.__name__,
            repr(self.block_type),
            repr(self.block_number),
            repr(self.block_proc_flags),
            len(self.data),
            repr(self.crc_type),
            (
                "0x{:08x}".format(self.crc_provided)
                if self.crc_provided is not None else "None"
            ),
        )

    @staticmethod
    def from_cbor(cbor_data):
        crc = None
        crc_type = CRCType(cbor_data[3])
        if crc_type == CRCType.NONE:
            assert len(cbor_data) == 5
        elif crc_type == CRCType.CRC32:
            assert len(cbor_data) == 6
            crc = struct.unpack('!I', cbor_data[5])[0]
        elif crc_type == CRCType.CRC16:
            assert len(cbor_data) == 6
            crc = struct.unpack('!H', cbor_data[5])[0]
        else:
            assert False

        return CanonicalBlock(
            block_type=BlockType(cbor_data[0]),
            data=cbor_data[4],
            block_number=cbor_data[1],
            block_proc_flags=BlockProcFlag(cbor_data[2]),
            crc_type=crc_type,
            crc_provided=crc,
        )


class PayloadBlock(CanonicalBlock):

    def __init__(self, data, **kwargs):
        super().__init__(BlockType.PAYLOAD,
                         data,
                         block_number=1,
                         **kwargs)


class CBORBlock(CanonicalBlock):

    def __init__(self, block_type, cbor_data, **kwargs):
        super().__init__(block_type, cbor.dumps(cbor_data), **kwargs)


# ----------------------
# Administrative Records
# ----------------------

class AdministrativeRecord(PayloadBlock):

    def __init__(self, bundle, record_type_code, record_data):
        record_data.extend([
            bundle.primary_block.source,
            bundle.primary_block.creation_time
        ])

        if bundle.is_fragmented:
            record_data.extend([
                bundle.primary_block.fragment_offset,
                bundle.primary_block.total_payload_length
            ])

        super().__init__(data=cbor.dumps([
            record_type_code,
            record_data
        ]))


class BundleStatusReport(AdministrativeRecord):

    def __init__(self, infos, reason, bundle, time=None):
        status_info = [
            [infos & StatusCode.RECEIVED_BUNDLE != 0],
            [infos & StatusCode.FORWARDED_BUNDLE != 0],
            [infos & StatusCode.DELIVERED_BUNDLE != 0],
            [infos & StatusCode.DELETED_BUNDLE != 0],
        ]

        if bundle.primary_block.has_flag(BundleProcFlag.REPORT_STATUS_TIME):
            if not time:
                time = datetime.now(timezone.utc)
            for info in status_info:
                if info[0]:
                    info.append(int(round((time - DTN_EPOCH).total_seconds())))

        record_data = [
            status_info,
            reason,
        ]

        super().__init__(
            bundle,
            record_type_code=RecordType.BUNDLE_STATUS_REPORT,
            record_data=record_data
        )

    @property
    def status_info(self):
        return self.data[1][0]

    def __repr__(self):
        return "<BundleStatusReport {!r}>".format(self.status_info)


# ----------------
# Extension Blocks
# ----------------

class PreviousNodeBlock(CBORBlock):

    def __init__(self, eid, **kwargs):
        super().__init__(BlockType.PREVIOUS_NODE, EID(eid), **kwargs)


class BundleAgeBlock(CBORBlock):
    """The Bundle Age block, block type 7, contains the number of milliseconds
    that have elapsed between the time the bundle was created and time at which
    it was most recently forwarded.

    Args:
        age (int): Age value in seconds
    """

    def __init__(self, age, **kwargs):
        super().__init__(BlockType.BUNDLE_AGE, int(age * 1000), **kwargs)


class HopCountBlock(CBORBlock):

    def __init__(self, hop_limit, hop_count, **kwargs):
        super().__init__(BlockType.HOP_COUNT, (hop_limit, hop_count), **kwargs)


class BundleMeta(type):
    """Metaclass for :class:`Bundle` providing the special initialization
    capability of a bundle object with a block list.

    This metaclass deconstructs the given block list to match the `__init__()`
    method of :class:`Bundle`. The first list element is treated as primary
    block, the last element as payload block. Every block in between these
    blocks are handled as extension block.
    """
    def __call__(cls, *args, **kwargs):
        # Special case:
        #     Bundle is created from a single list of blocks.
        #     Interpret the list elements by their position.
        if len(args) == 1:
            # Ensure there are enough block elements
            if len(args[0]) < 2:
                raise TypeError(
                    "Block list must contain at least two elements"
                )

            primary_block = args[0][0]     # First element
            payload_block = args[0][-1]    # Last element
            blocks = args[0][1:-1]         # Everything in between

            # Rearrange arguments to fit constructor
            args = (primary_block, payload_block, blocks)

        return super().__call__(*args, **kwargs)


class Bundle(object, metaclass=BundleMeta):
    """BPbis bundle data structure used for serialization

    A bundle is a list of blocks. The first list element is the primary block,
    the last list element is the payload block. Extension blocks are in between
    these two blocks.

    Args:
        primary_block (PrimaryBlock): Headers of the bundle
        payload_block (PayloadBlock): Payload of the bundle
        blocks (List[CanonicalBlock], optional): List of optional canonical
            (extension) blocks.

    There is a special constructor where you can create a bundle from a list of
    blocks. The blocks list is interpreted by there position in the list as
    desribed above. This capability is provided by the :class:`BundleMeta` meta
    class.

    .. code:: python

        bundle = Bundle([
            primary_block,
            hop_count,
            payload_block
        ])

    The :class:`Bundle` class supports the iterator protocol to iterate over
    every block in the same order as it would be encoded in CBOR.

    ..code:: python

        for i, block in enumerate(bundle):
            if i == 0:
                assert isinstance(block, PrimaryBlock)
    """
    def __init__(self, primary_block, payload_block, blocks=None):
        assert (
            primary_block.creation_time.time or
            blocks and
            any(b.block_type == BlockType.BUNDLE_AGE for b in blocks)
        ), "There must be a 'Bundle Age' block if the creation time is 0"

        self.primary_block = primary_block
        self.payload_block = payload_block
        self.blocks = []

        if blocks:
            for block in blocks:
                self.add(block)

    @property
    def is_fragmented(self):
        return self.primary_block.has_flag(BundleProcFlag.IS_FRAGMENT)

    def add(self, new_block):
        num = 1

        for block in self.blocks:
            message = "Block {} number already assigned".format(
                block.block_number
            )

            assert block.block_number != new_block.block_number, message

            # Search for a new block number if the block does not already
            # contain a block number
            num = max(num, block.block_number)

            # Assert unique block types
            if block.block_type == new_block.block_type:
                # Previous Node
                if block.block_type == BlockType.PREVIOUS_NODE:
                    raise ValueError(
                        "There must be only one 'Previous Node' block"
                    )
                # Hop Count
                elif block.block_type == BlockType.HOP_COUNT:
                    raise ValueError(
                        "There must be only one 'Hop Count' block"
                    )
                # Bundle Age
                elif block.block_type == BlockType.BUNDLE_AGE:
                    raise ValueError(
                        "There must be only one 'Bundle Age' block"
                    )

        if new_block.block_number is None:
            new_block.block_number = num + 1

        # Previous Node blocks must be the first blocks after the primary block
        if new_block.block_type == BlockType.PREVIOUS_NODE:
            self.blocks.insert(0, new_block)
        else:
            self.blocks.append(new_block)

    def hexlify(self):
        """Return the hexadecimal representation of the CBOR encoded bundle.

        Returns:
            bytes: CBOR encoded bundle as hex string
        """
        return hexlify(bytes(self))

    def __iter__(self):
        yield self.primary_block
        yield from self.blocks
        yield self.payload_block

    def __bytes__(self):
        # Header for indefinite array
        head = struct.pack('B', CBOR_ARRAY | 31)
        # Stop-code for indefinite array
        stop = b'\xff'
        return head + b''.join(bytes(block) for block in self) + stop

    def __repr__(self):
        return "{}(primary_block={}, payload_block={}, blocks=[{}])".format(
            self.__class__.__name__,
            repr(self.primary_block),
            repr(self.payload_block),
            ", ".join([repr(b) for b in self.blocks]),
        )

    @staticmethod
    def parse(data):
        cbor_data = cbor.loads(data)
        # At least a primary and a payload block are required
        assert len(cbor_data) >= 2
        primary_block = PrimaryBlock.from_cbor(cbor_data[0])
        payload_block = PayloadBlock.from_cbor(cbor_data[-1])
        blocks = [CanonicalBlock.from_cbor(e) for e in cbor_data[1:-1]]
        return Bundle(primary_block, payload_block, blocks)


_th_local = threading.local()


def next_sequence_number():
    seqnum = _th_local.__dict__.get("seqnum", 0)
    _th_local.__dict__["seqnum"] = seqnum + 1
    return seqnum


def reset_sequence_number():
    _th_local.__dict__["seqnum"] = 0


def create_bundle7(source_eid, destination_eid, payload,
                   report_to_eid=None, crc_type_primary=CRCType.CRC32,
                   creation_timestamp=None, sequence_number=None,
                   lifetime=300, flags=BlockProcFlag.NONE,
                   fragment_offset=None, total_adu_length=None,
                   hop_limit=None, hop_count=0, bundle_age=None,
                   previous_node_eid=None,
                   crc_type_canonical=CRCType.CRC16):
    """All-in-one function to encode a payload from a source EID
    to a destination EID as BPbis bundle.

    Args:
        source_eid (EID, str): Source endpoint address
        destination_eid (EID, str): Destination endpoint address
        report_to_eid (EID, str, optional): Endpoint address that should
            receive bundle status reports. If not given the null EID will be
            used.
        crc_type_primary (CRCType, optional): The kind of CRC used for the
            primary block.
        creation_timestamp (datetime, int, optional): Unix timestamp or
            timezone-aware datetime object when the bundle was created. If not
            given the current timestamp will be used.
        sequence_number (int, optional): Sequence number that is used for the
            bundle. If the device lacks a precise clock, this is the only
            source of information for differentiating two subsequent bundles.
            If None, the last thread-local sequence number will be used,
            incremented by one.
        lifetime (int, optional): Bundle lifetime in seconds
        flags (BlockProcFlag, optional): Bundle processing flags
        fragment_offset (int, optional): If the bundle is fragmented, use this
            offset. This value is only used if the bundle processing flag
            :attr:`BundleProcFlag.IS_FRAGMENT` is set.
        total_adu_length (int, optional): If the bundle is fragmented, use this
            to specify the total data length. This value is only used if the
            bundle processing flag :attr:`BundleProcFlag.IS_FRAGMENT` is set.
        hop_limit (int, optional): Maximal number of hops (intermediate DTN
            nodes) the bundle is allowed to use to reach its destination.
        hop_count (int, optional): Current hop count
        bundle_age (int, optional): Age of the bundle in seconds
        previous_node_eid (EID, str, optional): Address of the previous
            endpoint the bundle was received from Returns: bytes: CBOR encoded
            BPbis bundle
        crc_type_canonical (CRCType, optional): The kind of CRC used for the
            canonical blocks.
    """
    blocks = []

    if hop_limit is not None and hop_count is not None:
        blocks.append(
            HopCountBlock(hop_limit, hop_count, crc_type=crc_type_canonical)
        )
    if previous_node_eid is not None:
        blocks.append(
            PreviousNodeBlock(previous_node_eid, crc_type=crc_type_canonical)
        )
    if bundle_age is not None:
        blocks.append(
            BundleAgeBlock(bundle_age, crc_type=crc_type_canonical)
        )

    return Bundle(
        PrimaryBlock(
            bundle_proc_flags=flags,
            crc_type=crc_type_primary,
            destination=destination_eid,
            source=source_eid,
            report_to=report_to_eid,
            creation_time=CreationTimestamp(
                creation_timestamp,
                (
                    sequence_number
                    if sequence_number is not None
                    else next_sequence_number()
                ),
            ),
            lifetime=lifetime * 1000,
            fragment_offset=fragment_offset,
            total_payload_length=total_adu_length,
        ),
        PayloadBlock(
            payload,
            crc_type=crc_type_canonical,
        ),
        blocks,
    )


def serialize_bundle7(source_eid, destination_eid, payload,
                      report_to_eid=None, crc_type_primary=CRCType.CRC32,
                      creation_timestamp=None, sequence_number=None,
                      lifetime=300, flags=BlockProcFlag.NONE,
                      fragment_offset=None, total_adu_length=None,
                      hop_limit=None, hop_count=0, bundle_age=None,
                      previous_node_eid=None,
                      crc_type_canonical=CRCType.CRC16):
    """All-in-one function to encode a payload from a source EID
    to a destination EID as BPbis bundle.
    See create_bundle7 for a description of options."""
    return bytes(create_bundle7(
        source_eid, destination_eid, payload,
        report_to_eid, crc_type_primary,
        creation_timestamp, sequence_number,
        lifetime, flags,
        fragment_offset, total_adu_length,
        hop_limit, hop_count, bundle_age,
        previous_node_eid,
        crc_type_canonical
    ))
