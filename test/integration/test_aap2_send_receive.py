# SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
import pytest

from ud3tn_utils.aap2 import (
    AAP2UnixClient,
    AAPMessage,
    BundleADU,
    Keepalive,
    ResponseStatus,
)

from .helpers import (
    AAP2_AGENT_ID,
    AAP2_SECRET,
    AAP2_SOCKET,
    TEST_AAP2,
    UD3TN_EID,
)


def do_aap2_test(rpc_client, sub_client):
    print("Sending Keepalive message...")
    rpc_client.send(AAPMessage(keepalive=Keepalive()))
    response = rpc_client.receive_response()
    assert response.response_status == ResponseStatus.RESPONSE_STATUS_ACK

    print("Configuring the RPC client...")
    secret = rpc_client.configure(
        AAP2_AGENT_ID,
        subscribe=False,
        secret=AAP2_SECRET,
    )
    assert secret == AAP2_SECRET
    assert rpc_client.agent_id == AAP2_AGENT_ID
    assert rpc_client.eid == UD3TN_EID + AAP2_AGENT_ID

    print("Configuring the subscriber client...")
    secret2 = sub_client.configure(
        rpc_client.agent_id,
        subscribe=True,
        secret=secret,
    )
    assert secret2 == AAP2_SECRET
    assert sub_client.agent_id == rpc_client.agent_id

    print("Sending Keepalive message...")
    rpc_client.send(AAPMessage(keepalive=Keepalive()))
    response = rpc_client.receive_response()
    assert response.response_status == ResponseStatus.RESPONSE_STATUS_ACK

    print(f"Sending bundle to myself ({rpc_client.eid})...")
    payload = b"testbundle"
    rpc_client.send_adu(
        BundleADU(
            dst_eid=rpc_client.eid,
            payload_length=len(payload),
        ),
        payload,
    )
    response = rpc_client.receive_response()
    assert response.response_status == ResponseStatus.RESPONSE_STATUS_SUCCESS

    print("Receiving bundle...")
    msg = sub_client.receive_msg()
    assert msg is not None
    assert msg.WhichOneof("msg") == "adu"
    adu_msg, bundle_data = sub_client.receive_adu(msg.adu)
    sub_client.send_response_status(
        ResponseStatus.RESPONSE_STATUS_SUCCESS
    )
    print("Bundle received from {}, payload = {}".format(
        adu_msg.src_eid, bundle_data
    ))
    assert bundle_data == payload
    assert adu_msg.payload_length == len(payload)
    assert adu_msg.src_eid == rpc_client.eid
    # Test bundle headers returned when sending it
    rbdl = response.bundle_headers
    assert adu_msg.src_eid == rbdl.src_eid
    assert adu_msg.dst_eid == rbdl.dst_eid
    assert adu_msg.creation_timestamp_ms == rbdl.creation_timestamp_ms
    assert adu_msg.sequence_number == rbdl.sequence_number
    assert adu_msg.payload_length == rbdl.payload_length


@pytest.mark.skipif(not TEST_AAP2, reason="TEST_AAP2 disabled via environment")
def test_aap2_send_receive():
    rpc_client = AAP2UnixClient(address=AAP2_SOCKET)
    sub_client = AAP2UnixClient(address=AAP2_SOCKET)
    with rpc_client:
        with sub_client:
            do_aap2_test(rpc_client, sub_client)
