import time
import math

import pytest

from ud3tn_utils.config import (
    ConfigMessage, make_contact, serialize_set_time_cmd
)

from pyd3tn.bundle7 import serialize_bundle7
from pyd3tn.bundle6 import serialize_bundle6
from pyd3tn.mtcp import MTCPConnection
from pyd3tn.spp import TCPSPPConnection
from pyd3tn.tcpcl import TCPCLConnection

from .helpers import (
    TESTED_CLAS,
    UD3TN_CONFIG_EP,
    UD3TN_MANAGEMENT_EP,
    UD3TN_HOST,
    TCPSPP_PORT,
    TCPCL_PORT,
    SMTCP_PORT,
    SPP_USE_CRC,
    TCP_TIMEOUT,
    STM32_TIMEOUT,
    validate_bundle6,
    validate_bundle7,
    send_delete_gs,
)

SENDING_GS_DEF = ("dtn://sender.dtn", "sender")
RECEIVING_GS_DEF = ("dtn://receiver.dtn", "receiver")

SENDING_CONTACT = (1, 1, 1000)
RECEIVING_CONTACT = (3, 1, 1000)
BUNDLE_SIZE = 200
PAYLOAD_DATA = b"\x42" * BUNDLE_SIZE


def _wait_for_even_timestamp():
    # Wait until TARGET_OFFSET before the next second starts (for syncing time
    # using second granularity)
    # A high offset is required for STM32 which can be quite slow sometimes...
    TARGET_OFFSET = 0.9
    cur_time = time.time()
    next_second = math.ceil(cur_time)
    if next_second - cur_time < TARGET_OFFSET:
        next_second += 1
    time.sleep(next_second - cur_time - TARGET_OFFSET)
    return int(next_second)


def perform_basic_test(connection_obj, serialize_func, validate_func, cla_str,
                       sync_time=False):
    with connection_obj as conn:
        outgoing_eid, outgoing_claaddr = SENDING_GS_DEF
        incoming_eid, incoming_claaddr = RECEIVING_GS_DEF
        # Configure current time if requested
        if sync_time:
            ts = _wait_for_even_timestamp()
            conn.send_bundle(serialize_func(
                outgoing_eid,
                UD3TN_MANAGEMENT_EP,
                serialize_set_time_cmd(ts),
            ))
        # Configure contact during which we send a bundle
        conn.send_bundle(serialize_func(
            outgoing_eid,
            UD3TN_CONFIG_EP,
            bytes(ConfigMessage(
                outgoing_eid,
                cla_str + ":" + outgoing_claaddr,
                contacts=[
                    make_contact(*SENDING_CONTACT),
                ],
            )),
        ))
        # Configure contact during which we want to receive the bundle
        conn.send_bundle(serialize_func(
            outgoing_eid,
            UD3TN_CONFIG_EP,
            bytes(ConfigMessage(
                incoming_eid,
                cla_str + ":" + incoming_claaddr,
                contacts=[
                    make_contact(*RECEIVING_CONTACT),
                ],
            )),
        ))
        try:
            # Wait until first contact starts and send bundle
            time.sleep(SENDING_CONTACT[0])
            conn.send_bundle(serialize_func(
                outgoing_eid,
                incoming_eid,
                PAYLOAD_DATA,
            ))
            # Wait until second contact starts and try to receive bundle
            time.sleep(RECEIVING_CONTACT[0] - SENDING_CONTACT[0])
            bdl = conn.recv_bundle()
            validate_func(bdl, PAYLOAD_DATA)
            time.sleep(RECEIVING_CONTACT[1])
        finally:
            send_delete_gs(conn, serialize_func, (outgoing_eid, incoming_eid))


@pytest.mark.skipif("tcpspp" not in TESTED_CLAS, reason="not selected")
def test_send_receive_spp_bundle6():
    perform_basic_test(
        TCPSPPConnection(
            UD3TN_HOST,
            TCPSPP_PORT,
            SPP_USE_CRC,
            timeout=TCP_TIMEOUT,
        ),
        serialize_bundle6,
        validate_bundle6,
        "tcpspp",
    )


@pytest.mark.skipif("tcpspp" not in TESTED_CLAS, reason="not selected")
def test_send_receive_spp_bundle7():
    perform_basic_test(
        TCPSPPConnection(
            UD3TN_HOST,
            TCPSPP_PORT,
            SPP_USE_CRC,
            timeout=TCP_TIMEOUT,
        ),
        serialize_bundle7,
        validate_bundle7,
        "tcpspp",
    )


@pytest.mark.skipif("smtcp" not in TESTED_CLAS, reason="not selected")
def test_send_receive_smtcp_bundle6():
    perform_basic_test(
        MTCPConnection(UD3TN_HOST, SMTCP_PORT, timeout=TCP_TIMEOUT),
        serialize_bundle6,
        validate_bundle6,
        "smtcp",
    )


@pytest.mark.skipif("smtcp" not in TESTED_CLAS, reason="not selected")
def test_send_receive_smtcp_bundle7():
    perform_basic_test(
        MTCPConnection(UD3TN_HOST, SMTCP_PORT, timeout=TCP_TIMEOUT),
        serialize_bundle7,
        validate_bundle7,
        "smtcp",
    )


@pytest.mark.skipif("tcpclv3" not in TESTED_CLAS, reason="not selected")
def test_send_receive_tcpcl_bundle6():
    perform_basic_test(
        TCPCLConnection(
            "dtn://receiver.dtn",
            UD3TN_HOST,
            TCPCL_PORT,
            timeout=TCP_TIMEOUT,
        ),
        serialize_bundle6,
        validate_bundle6,
        "tcpclv3",
    )


@pytest.mark.skipif("tcpclv3" not in TESTED_CLAS, reason="not selected")
def test_send_receive_tcpcl_bundle7():
    perform_basic_test(
        TCPCLConnection(
            "dtn://receiver.dtn",
            UD3TN_HOST,
            TCPCL_PORT,
            timeout=TCP_TIMEOUT,
        ),
        serialize_bundle7,
        validate_bundle7,
        "tcpclv3",
    )


@pytest.mark.skipif("usbotg" not in TESTED_CLAS, reason="not selected")
def test_send_receive_usbotg_bundle6():
    perform_basic_test(
        MTCPConnection(UD3TN_HOST, SMTCP_PORT, timeout=STM32_TIMEOUT),
        serialize_bundle6,
        validate_bundle6,
        "usbotg",
        sync_time=True,
    )


@pytest.mark.skipif("usbotg" not in TESTED_CLAS, reason="not selected")
def test_send_receive_usbotg_bundle7():
    perform_basic_test(
        MTCPConnection(UD3TN_HOST, SMTCP_PORT, timeout=STM32_TIMEOUT),
        serialize_bundle7,
        validate_bundle7,
        "usbotg",
        sync_time=True,
    )
