import os

from ud3tn_utils.config import ConfigMessage, RouterCommand
from pyd3tn.bundle7 import Bundle, CRCType

USER_SELECTED_CLA = os.environ.get("CLA", None)
TESTED_CLAS = [USER_SELECTED_CLA] if USER_SELECTED_CLA else [
    "tcpclv3",
    "tcpspp",
    "smtcp",
]

UD3TN_HOST = "localhost"
TCPSPP_PORT = 4223
TCPCL_PORT = 4556
SMTCP_PORT = 4222
# MTCP_PORT = 4224

UD3TN_EID = "dtn://ud3tn.dtn"
UD3TN_CONFIG_EP = UD3TN_EID + "/config"
UD3TN_MANAGEMENT_EP = UD3TN_EID + "/management"

TEST_SCRIPT_EID = "dtn://manager.dtn"

SPP_USE_CRC = os.environ.get("TCPSPP_CRC_ENABLED", "1") == "1"
TCP_TIMEOUT = 1.
STM32_TIMEOUT = 2.


def validate_bundle7(bindata, expected_payload=None):
    b = Bundle.parse(bindata)
    print("Received bundle: {}".format(repr(b)))
    crc_valid = True
    for block in b:
        print((
            "Block {} {} CRC: provided={}, calculated={}"
        ).format(
            block.block_number,
            repr(block.block_type),
            (
                "0x{:08x}".format(block.crc_provided)
                if block.crc_provided else "None"
            ),
            (
                "0x{:08x}".format(block.calculate_crc())
                if block.crc_type != CRCType.NONE else "None"
            ),
        ))
        if block.crc_type != CRCType.NONE:
            crc_valid &= block.crc_provided == block.calculate_crc()
    assert crc_valid
    if expected_payload:
        assert b.payload_block.data == expected_payload
    return b


def validate_bundle6(bindata, expected_payload=None):
    assert bindata[0] == 0x06
    print("Received RFC 5050 binary data: {}".format(repr(bindata)))
    # TODO: Add a proper validation
    if expected_payload:
        assert expected_payload in bindata
    return bindata


def send_delete_gs(conn, serialize_func, gs_iterable):
    for eid in gs_iterable:
        print("Sending DELETE command for {}".format(eid))
        conn.send_bundle(serialize_func(
            TEST_SCRIPT_EID,
            UD3TN_CONFIG_EP,
            bytes(ConfigMessage(
                eid,
                "NULL",
                type=RouterCommand.DELETE,
            ))
        ))
