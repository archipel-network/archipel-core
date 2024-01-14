# SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
import pytest
import time

from pyd3tn.helpers import UNIX_TO_DTN_OFFSET
from ud3tn_utils.aap import (
    AAPUnixClient,
    AAPTCPClient,
    AAPMessage,
    AAPMessageType,
)

from .helpers import (
    AAP_USE_TCP,
    AAP_SOCKET,
    AAP_AGENT_ID,
    TEST_AAP,
    UD3TN_EID,
)


def do_aap_test(aap_client):
    print("Sending PING message...")
    aap_client.ping()

    print("Sending REGISTER message...")
    aap_client.register(AAP_AGENT_ID)
    assert aap_client.eid == UD3TN_EID + AAP_AGENT_ID

    print("Sending PING message...")
    aap_client.ping()

    print(f"Sending bundle to myself ({aap_client.eid})...")
    reply = aap_client.send_str(
        aap_client.eid,
        "42"
    )
    assert reply.msg_type == AAPMessageType.SENDCONFIRM
    bundle_id = reply.decode_bundle_id()
    assert len(bundle_id) == 2
    assert bundle_id[1] == 1
    cur_dtn_ts_ms = (time.time() - UNIX_TO_DTN_OFFSET) * 1000
    print("Timestamps: bundle: {}, cur: {}".format(
        bundle_id[0],
        cur_dtn_ts_ms,
    ))
    assert abs(bundle_id[0] - cur_dtn_ts_ms) < 100
    assert reply.bundle_id == AAPMessage.encode_bundle_id(
        bundle_id[0],
        bundle_id[1],
    )

    msg_bdl = aap_client.receive()
    assert msg_bdl.msg_type == AAPMessageType.RECVBUNDLE
    print("Bundle received from {}, payload = {}".format(
        msg_bdl.eid, msg_bdl.payload
    ))
    assert msg_bdl.payload == b"42"


@pytest.mark.skipif(not TEST_AAP, reason="TEST_AAP disabled via environment")
def test_aap_send_receive():
    if AAP_USE_TCP:
        with AAPTCPClient(address=AAP_SOCKET) as aap_client:
            do_aap_test(aap_client)
    else:
        with AAPUnixClient(address=AAP_SOCKET) as aap_client:
            do_aap_test(aap_client)
