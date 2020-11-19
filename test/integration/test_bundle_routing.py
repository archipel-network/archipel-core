import time
import random
import copy

import pytest

from ud3tn_utils.config import ConfigMessage, make_contact

from pyd3tn.bundle7 import serialize_bundle7, BundleProcFlag
from pyd3tn.spp import TCPSPPConnection

from .helpers import (
    TESTED_CLAS,
    UD3TN_CONFIG_EP,
    UD3TN_HOST,
    TCPSPP_PORT,
    SPP_USE_CRC,
    TCP_TIMEOUT,
    TEST_SCRIPT_EID,
    validate_bundle7,
    send_delete_gs,
)

CLA_ADDR = {
    TEST_SCRIPT_EID: "tcpspp:manager.dtn",
    "dtn:1": "tcpspp:1.dtn",
    "dtn:2": "tcpspp:2.dtn",
    "dtn:3": "tcpspp:3.dtn",
}

BUNDLE_SIZE = 2000

# with_gs, from, duration, rate, sent_bdls, rcvd_bdls
# bundle: from/to, fragmented?
CONTACT_LIST = [
    (
        "dtn:1", 1, 1, 9600,
        ["dtn:3"] * 4,
        [],
    ),
    (
        "dtn:2", 3, 1, 9600,
        ["dtn:3"] * 3,
        [],
    ),
    (
        "dtn:3", 5, 1, 9600,
        ["dtn:1", "dtn:1", "dtn:2"],
        [("dtn:1", False)] * 4 + [("dtn:2", True)],
    ),
    (
        "dtn:1", 7, 1, 9600,
        ["dtn:3"],
        [("dtn:3", False)] * 2,
    ),
    (
        "dtn:2", 9, 1, 9600,
        [],
        [("dtn:3", False)],
    ),
    (
        "dtn:3", 11, 1, 9600,
        ["dtn:1"] * 3 + ["dtn:2"],
        [("dtn:2", True), ("dtn:1", False)] + [("dtn:2", False)] * 2,
    ),
    (
        "dtn:1", 13, 1, 9600,
        [],
        [("dtn:3", False)] * 3,
    ),
    (
        "dtn:2", 15, 1, 9600,
        [],
        [("dtn:3", False)],
    ),
    (
        "dtn:3", 17, 1, 9600,
        [],
        [],
    ),
]


def configure_contacts(conn, serialize_func, contact_list):
    for eid, start, duration, rate, tx, rx in CONTACT_LIST:
        conn.send_bundle(serialize_func(
            TEST_SCRIPT_EID,
            UD3TN_CONFIG_EP,
            bytes(ConfigMessage(
                eid,
                CLA_ADDR[eid],
                contacts=[
                    make_contact(start, duration, rate),
                ],
            )),
        ))


def send_bundles(conn, serialize_func, sender_eid, receiver_list, pl_size):
    sent = []
    for rcv_eid in receiver_list:
        print("Sending bundle to {}".format(rcv_eid))
        pl = bytes(random.getrandbits(8) for _ in range(pl_size))
        bdl = serialize_func(sender_eid, rcv_eid, pl)
        conn.send_bundle(bdl)
        sent.append(bdl)
    return sent


def receive_and_check(conn, validate_func, is_frag_func, expected_bundles):
    # Receive bundles
    rcvd_bundles = []
    for i, _ in enumerate(expected_bundles):
        print("Waiting to receive bundle #{}...".format(i))
        try:
            bdl = conn.recv_bundle()
        except Exception:
            print("Reception did not finish.")
            raise
        decoded = validate_func(bdl, None)
        print("Received bundle with {} bytes data, sender = {}".format(
            len(decoded.payload_block.data),
            decoded.primary_block.source,
        ))
        rcvd_bundles.append(decoded)
    # Check what we received was expected
    # Format of expected_bundles: Tuples of (sender_eid, is_fragmented)
    rx_check = copy.deepcopy(expected_bundles)
    for bdl in rcvd_bundles:
        tup = (str(bdl.primary_block.source), is_frag_func(bdl))
        print("Trying to find expectation: {}".format(tup))
        assert tup in rx_check, (
            "we did not receive an expected bundle"
        )
        rx_check.remove(tup)
    assert not rx_check, (
        "we received less than the expected bundles"
    )


def perform_routing_test(connection_obj, serialize_func, validate_func,
                         is_frag_func):
    # estimate target pl size by subtracting estimated header size
    pl_size = BUNDLE_SIZE - (
        len(serialize_func("dtn:1", "dtn:2", b"\x42" * BUNDLE_SIZE)) -
        BUNDLE_SIZE
    )
    with connection_obj as conn:
        configure_contacts(conn, serialize_func, CONTACT_LIST)
        try:
            cur_off = 0
            cur_time = time.time()
            for eid, start, duration, rate, tx, rx in CONTACT_LIST:
                # Wait until start (as we do not know how long the previous
                # iteration took, we have to calculate the time until the
                # start of the next contact based on the last offset)
                next_time = cur_time + start - cur_off
                assert next_time > time.time()
                time.sleep(next_time - time.time())
                cur_off = start
                cur_time = next_time
                print("[{}] Handling contact at {} with {}".format(
                    cur_time, cur_off, eid,
                ))
                # TODO: track sent bundles using return value
                send_bundles(conn, serialize_func, eid, tx, pl_size)
                receive_and_check(conn, validate_func, is_frag_func, rx)
        finally:
            # Cleanup
            next_time = (
                cur_time - cur_off +
                # 1 second past end of last contact
                CONTACT_LIST[-1][1] + CONTACT_LIST[-1][2] + 1
            )
            print("Waiting {}s for completion of contacts...".format(
                next_time - time.time()
            ))
            time.sleep(next_time - time.time())
            send_delete_gs(conn, serialize_func, ("dtn:1", "dtn:2", "dtn:3"))


def bundle7_is_fragment(bdl):
    return (
        bdl.primary_block.bundle_proc_flags & BundleProcFlag.IS_FRAGMENT
    ) != 0


@pytest.mark.skipif("tcpspp" not in TESTED_CLAS, reason="not selected")
def test_routing_spp_bundle7():
    perform_routing_test(
        TCPSPPConnection(
            UD3TN_HOST,
            TCPSPP_PORT,
            SPP_USE_CRC,
            timeout=TCP_TIMEOUT,
        ),
        serialize_bundle7,
        validate_bundle7,
        bundle7_is_fragment,
    )
