import enum
import struct
import socket
import logging
from datetime import datetime, timedelta, timezone

from .crc import crc16_ccitt_false
from .helpers import CCSDS_EPOCH, sock_recv_raw

logger = logging.getLogger(__name__)

SPP_MAX_APID = 0x7ff
SPP_MAX_SEGMENT_NUMBER = 0x3fff
SPP_MAX_DATA_LENGTH = 65536

SPP_PRIMARY_HEADER_LENGTH = 6
SPP_PH_P1_VERSION_MASK = 0xe000
SPP_PH_EARLY_VERSION_MASK = SPP_PH_P1_VERSION_MASK >> 8
SPP_PH_P1_TYPE_MASK = 0x1000
SPP_PH_P1_SECONDARY_HEADER_FLAG_MASK = 0x0800
SPP_PH_P1_APID_MASK = 0x07ff
SPP_PH_P2_SEQUENCE_FLAGS_MASK = 0xc000
SPP_PH_P2_SEQUENCE_FLAGS_SHIFT = 14
SPP_PH_P2_SEQUENCE_CNT_MASK = 0x3fff

SPP_TC_UNSEG_BASE_UNIT_LONGP_THRESHOLD = 4
SPP_TC_UNSEG_FRAC_LONGP_THRESHOLD = 3


class SPPSegmentStatus(enum.IntFlag):
    ContinuationSegment = 0,
    FirstSegment = 1,
    LastSegment = 2,
    Unsegmented = 3,


class SPPPrimaryHeaderFlags(enum.IntFlag):
    HasSecondaryHeader = SPP_PH_P1_SECONDARY_HEADER_FLAG_MASK,
    PacketTypeIsRequest = SPP_PH_P1_TYPE_MASK,


class SPPTimecodeType(enum.IntEnum):
    UnsegmentedCCSDSEpoch = 0x1,
    UnsegmentedCustomEpoch = 0x2,
    DaySegmented = 0x4,
    CalendarSegmented = 0x5,
    Custom = 0x6,
    Unknown = 0x7,


class SPPTimecode(object):

    def __init__(self, dt=None, fractional=0,
                 timecode_type=SPPTimecodeType.UnsegmentedCCSDSEpoch,
                 base_unit_octets=4, fractional_octets=0,
                 has_preamble=True):
        self.dt = dt or datetime.now(timezone.utc)
        self.fractional = fractional
        self.timecode_type = timecode_type
        self.base_unit_octets = base_unit_octets
        self.fractional_octets = fractional_octets
        self.has_preamble = has_preamble

    def length(self):
        return (
            (1 if self.has_preamble else 0) +
            self.base_unit_octets +
            self.fractional_octets
        )

    def __bytes__(self):
        assert self.has_preamble

        # We do not support other configurations for now.
        assert self.timecode_type == SPPTimecodeType.UnsegmentedCCSDSEpoch

        # We do not allow timecodes longer than what can be represented in
        # the preamble.
        assert self.base_unit_octets > 0
        assert self.fractional_octets >= 0
        assert self.base_unit_octets <= SPP_TC_UNSEG_BASE_UNIT_LONGP_THRESHOLD
        assert self.fractional_octets <= SPP_TC_UNSEG_FRAC_LONGP_THRESHOLD
        preamble = 0b0_000_00_00  # second preamble bit = 0b0
        preamble |= int(self.timecode_type) << 4
        preamble |= (self.base_unit_octets - 1) << 2
        preamble |= self.fractional_octets

        ret = bytes([preamble])
        ret += int((self.dt - CCSDS_EPOCH).total_seconds()).to_bytes(
            self.base_unit_octets,
            "big"
        )
        if self.fractional_octets:
            ret += int(self.fractional).to_bytes(
                self.fractional_octets,
                "big"
            )

        return ret

    @staticmethod
    def parse(data, has_preamble=True):
        assert has_preamble

        preamble = data[0]
        timecode_type = (preamble & 0b0_111_00_00) >> 4
        base_unit_octets = ((preamble & 0b0_000_11_00) >> 2) + 1
        fractional_octets = (preamble & 0b0_000_00_11)
        assert timecode_type == SPPTimecodeType.UnsegmentedCCSDSEpoch

        fractional_offset = 1 + base_unit_octets
        fractional_end = fractional_offset + fractional_octets

        dt = CCSDS_EPOCH + timedelta(seconds=int.from_bytes(
            data[1:fractional_offset],
            "big"
        ))
        fractional = int.from_bytes(
            data[fractional_offset:fractional_end],
            "big"
        )

        return SPPTimecode(
            dt=dt,
            fractional=fractional,
            timecode_type=SPPTimecodeType(timecode_type),
            base_unit_octets=base_unit_octets,
            fractional_octets=fractional_octets,
        ), fractional_end

    def __repr__(self):
        return (
            "SPPTimecode(dt={}, fractional={}, timecode_type={}, "
            "base_unit_octets={}, fractional_octets={}, has_preamble={})"
        ).format(
            repr(self.dt),
            self.fractional,
            repr(self.timecode_type),
            self.base_unit_octets,
            self.fractional_octets,
            self.has_preamble,
        )


class SPPPacketHeader(object):

    def __init__(self, payload_length=0, is_request=False, apid=0x1,
                 segment_status=SPPSegmentStatus.Unsegmented,
                 segment_number=0, timecode=None, ancillary_data=None):
        self.payload_length = payload_length
        self.is_request = is_request
        assert apid >= 0
        assert apid <= SPP_MAX_APID
        self.apid = apid
        self.segment_status = segment_status
        assert segment_number >= 0
        assert segment_number <= SPP_MAX_SEGMENT_NUMBER
        self.segment_number = segment_number
        self.timecode = timecode
        self.ancillary_data = ancillary_data

    def data_length(self):
        return self.payload_length + self.secondary_header_length()

    def secondary_header_length(self):
        return (
            len(self.ancillary_data) if self.ancillary_data else 0 +
            self.timecode.length() if self.timecode else 0
        )

    def total_header_length(self):
        return SPP_PRIMARY_HEADER_LENGTH + self.secondary_header_length()

    def minimum_payload_length(self):
        if self.secondary_header_length() > 0:
            return 0
        return 1

    def __bytes__(self):
        assert self.payload_length >= self.minimum_payload_length()

        data_length = self.data_length()
        assert data_length <= SPP_MAX_DATA_LENGTH

        part1 = self.apid
        if self.is_request:
            part1 |= SPP_PH_P1_TYPE_MASK
        # Has secondary header?
        if self.ancillary_data is not None or self.timecode is not None:
            part1 |= SPP_PH_P1_SECONDARY_HEADER_FLAG_MASK
        part2 = (
            (self.segment_status << SPP_PH_P2_SEQUENCE_FLAGS_SHIFT) |
            self.segment_number
        )

        primary_header = [
            struct.pack('!H', part1),
            struct.pack('!H', part2),
            struct.pack('!H', self.data_length() - 1),
        ]

        secondary_header = []

        if self.timecode is not None:
            secondary_header.append(bytes(self.timecode))

        # NOTE that the length of the ancillary data field has to be fixed!
        if self.ancillary_data is not None:
            secondary_header.append(bytes(self.ancillary_data))

        return b"".join(primary_header + secondary_header)

    @staticmethod
    def preparse_data_length(primary_header_bytes):
        part1, _, data_length = struct.unpack('!HHH', primary_header_bytes[:6])
        has_secondary_header = (
            part1 & SPP_PH_P1_SECONDARY_HEADER_FLAG_MASK
        ) != 0
        return data_length + 1, has_secondary_header

    @staticmethod
    def parse(data, timecode_used=False, ancillary_data_length=0):
        part1, part2, data_length = struct.unpack('!HHH', data[:6])

        assert (part1 & 0b1110_0000_0000_0000) == 0
        is_request = (part1 & SPP_PH_P1_TYPE_MASK) != 0
        has_secondary_header = (
            part1 & SPP_PH_P1_SECONDARY_HEADER_FLAG_MASK
        ) != 0
        apid = part1 & SPP_PH_P1_APID_MASK

        segment_status = (
            part2 & SPP_PH_P2_SEQUENCE_FLAGS_MASK
        ) >> SPP_PH_P2_SEQUENCE_FLAGS_SHIFT
        segment_number = part2 & SPP_PH_P2_SEQUENCE_CNT_MASK

        data_length += 1
        payload_offset = SPP_PRIMARY_HEADER_LENGTH

        timecode = None
        ancillary_data = None
        if has_secondary_header:
            if timecode_used:
                assert has_secondary_header
                timecode, timecode_len = SPPTimecode.parse(
                    data[payload_offset:]
                )
                payload_offset += timecode_len
            if ancillary_data_length:
                assert has_secondary_header
                ancillary_data_start = payload_offset
                payload_offset += ancillary_data_length
                ancillary_data = data[ancillary_data_start:payload_offset]

        payload_length = (
            data_length - (payload_offset - SPP_PRIMARY_HEADER_LENGTH)
        )

        return SPPPacketHeader(
            payload_length=payload_length,
            is_request=is_request,
            apid=apid,
            segment_status=SPPSegmentStatus(segment_status),
            segment_number=segment_number,
            timecode=timecode,
            ancillary_data=ancillary_data,
        ), payload_offset

    def __repr__(self):
        return (
            "SPPPacketHeader(payload_length={}, is_request={}, apid=0x{:02x}, "
            "segment_status={}, segment_number={}, timecode={}, "
            "ancillary_data={})"
        ).format(
            self.payload_length,
            self.is_request,
            self.apid,
            self.segment_status,
            self.segment_number,
            self.timecode,
            "<len={}>".format(
                len(self.ancillary_data)
            ) if self.ancillary_data else "None",
        )


class SPPPacket(object):

    def __init__(self, header, payload, has_crc=False, crc_provided=None):
        header.payload_length = len(payload)
        if has_crc:
            header.payload_length += 2
        self.header = header
        self.payload = payload
        self.has_crc = has_crc
        self.crc_provided = crc_provided

    def crc(self, binary=None):
        return crc16_ccitt_false(binary or (bytes(self.header) + self.payload))

    def __bytes__(self):
        packet = bytes(self.header) + self.payload
        if self.has_crc:
            packet += struct.pack("!H", self.crc(packet))
        return packet

    @staticmethod
    def parse(data, has_crc=False, timecode_used=False,
              ancillary_data_length=0):
        header, header_end = SPPPacketHeader.parse(
            data,
            timecode_used=timecode_used,
            ancillary_data_length=ancillary_data_length,
        )

        payload_end = header_end + header.payload_length
        payload = data[header_end:payload_end]

        crc = None
        if has_crc:
            crc = struct.unpack("!H", payload[-2:])[0]
            payload = payload[:-2]

        return SPPPacket(header, payload, has_crc, crc), payload_end

    def __repr__(self):
        return (
            "SPPPacket(header={}, payload=<length={}>, has_crc={}, "
            "crc_provided={})"
        ).format(
            repr(self.header),
            len(self.payload),
            self.has_crc,
            "None" if self.crc_provided is None else "<0x{:04x}, {}>".format(
                self.crc_provided,
                "OK" if self.crc() == self.crc_provided else "FAIL",
            ),
        )


class TCPSPPConnection(object):
    """TCPSPP connection to a remote DTN node.

    The connection supports the context manager protocol to ensure a proper
    shutdown of the connection in case of any error:

    .. code:: python

        from pyd3tn.spp import TCPSPPConnection

        with TCPSPPConnection('127.0.0.1', 4223) as conn:
            conn.send(bundle)

    Args:
        host (str): Host to connect to
        port (int): TCP port to connect to
        use_crc (bool): Whether or not the SPP CRC is used
        timeout (float): Timeout for receiving bundles (None for infinite)
    """
    def __init__(self, host, port, use_crc=False, timeout=None):
        self.host = host
        self.port = port
        self.use_crc = use_crc
        self.timeout = timeout
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    def __enter__(self):
        self.connect()
        return self

    def __exit__(self, *args):
        self.disconnect()

    def connect(self):
        """Establish a TCPSPP connection to a remote peer."""
        self.sock.connect((self.host, self.port))
        self.sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        logger.debug("TCPSPP: Connected to %s:%d", self.host, self.port)

    def disconnect(self):
        """Close a TCPSPP connection."""
        logger.debug(
            "TCPSPP: Terminate connection to %s:%d",
            *self.sock.getpeername(),
        )
        self.sock.shutdown(socket.SHUT_RDWR)
        self.sock.close()

    def send_bundle(self, bundle):
        """Send a single bundle to the remote peer.

        Args:
            bundle (bytes): Serialized bundle
        """
        raw = bytes(SPPPacket(
            SPPPacketHeader(
                timecode=SPPTimecode(),
            ),
            bundle,
            has_crc=self.use_crc,
        ))
        return self.sock.sendall(raw)

    def recv_bundle(self):
        """Receives a single bundle from the remote peer.

        Returns:
            bundle (bytes): Serialized bundle
        """
        hdr_raw = sock_recv_raw(self.sock, 6, self.timeout)
        data_length, has_secondary = SPPPacketHeader.preparse_data_length(
            hdr_raw
        )
        data_plus_secondary = sock_recv_raw(
            self.sock,
            data_length,
            self.timeout,
        )
        packet, _ = SPPPacket.parse(
            hdr_raw + data_plus_secondary,
            timecode_used=True,
            has_crc=self.use_crc,
        )
        if self.use_crc:
            assert packet.crc_provided == packet.crc()
        return packet.payload
