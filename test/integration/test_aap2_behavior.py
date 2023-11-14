# SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
import pytest
import select
import time

from ud3tn_utils.aap2 import (
    AAP2UnixClient,
    AAPMessage,
    BundleADU,
    Keepalive,
    ResponseStatus,
    AAP2OperationFailed,
)

from .helpers import (
    AAP2_AGENT_ID,
    AAP2_SECRET,
    AAP2_SOCKET,
    TEST_AAP2,
)

WAIT_TIMEOUT = 3
TIMING_MARGIN = 0.5
AAP2_AGENT_TIMEOUT = 1


def _wait_for_data(sock, timeout=WAIT_TIMEOUT):
    rl, _, _ = select.select([sock], [],  [], timeout)
    assert len(rl) > 0, "timed out"


def _wait_until_connection_closed(sock, timeout=WAIT_TIMEOUT):
    _wait_for_data(sock, timeout)
    try:
        rv = sock.recv(1)
        assert not rv
    except ConnectionResetError:
        pass


@pytest.mark.skipif(not TEST_AAP2, reason="TEST_AAP2 disabled via environment")
def test_aap2_re_register():
    # Test that we can register again with the same agent but a different
    # secret after closing the connection.
    client = AAP2UnixClient(address=AAP2_SOCKET)
    with client:
        client.configure(
            AAP2_AGENT_ID + "-b",
            subscribe=False,
            secret=AAP2_SECRET,
        )
        client.send(AAPMessage(keepalive=Keepalive()))
        response = client.receive_response()
        assert response.response_status == ResponseStatus.RESPONSE_STATUS_ACK
    # Unfortunately there is a race condition with detecting socket closure -
    # uD3TN might get notified of the dropped connection later than it receives
    # the configuration message for the next connection.
    time.sleep(1)
    client = AAP2UnixClient(address=AAP2_SOCKET)
    with client:
        client.configure(
            AAP2_AGENT_ID + "-b",
            subscribe=False,
            secret=AAP2_SECRET + "123",
        )
        client.send(AAPMessage(keepalive=Keepalive()))
        response = client.receive_response()
        assert response.response_status == ResponseStatus.RESPONSE_STATUS_ACK


@pytest.mark.skipif(not TEST_AAP2, reason="TEST_AAP2 disabled via environment")
def test_aap2_close_on_send_to_sub():
    sub_client = AAP2UnixClient(address=AAP2_SOCKET)
    with sub_client:
        sub_client.configure(
            AAP2_AGENT_ID + "-b2",
            subscribe=True,
            secret=AAP2_SECRET,
        )
        # Test that a subscriber terminates if we send something without
        # receiving a call previously (enforcement of comm. direction).
        sub_client.send_response_status(ResponseStatus.RESPONSE_STATUS_SUCCESS)
        _wait_until_connection_closed(sub_client.socket)


@pytest.mark.skipif(not TEST_AAP2, reason="TEST_AAP2 disabled via environment")
def test_aap2_reconfigure():
    client = AAP2UnixClient(address=AAP2_SOCKET)
    with client:
        client.configure(
            AAP2_AGENT_ID + "-b3",
            subscribe=False,
            secret=AAP2_SECRET,
        )
        client.configure(
            AAP2_AGENT_ID + "-b4",
            subscribe=False,
            secret=AAP2_SECRET,
        )
        assert client.agent_id == AAP2_AGENT_ID + "-b4"


@pytest.mark.skipif(not TEST_AAP2, reason="TEST_AAP2 disabled via environment")
def test_aap2_fail_on_wrong_secret():
    rpc_client = AAP2UnixClient(address=AAP2_SOCKET)
    sub_client = AAP2UnixClient(address=AAP2_SOCKET)
    with rpc_client:
        with sub_client:
            s1 = rpc_client.configure(
                AAP2_AGENT_ID + "-b5",
                subscribe=False,
            )
            # Test reconfiguration with correct secret returning it correctly
            s2 = sub_client.configure(
                AAP2_AGENT_ID + "-b5",
                subscribe=True,
                secret=s1,
            )
            assert s1 == s2
            # Test providing invalid secret
            try:
                rpc_client.configure(
                    AAP2_AGENT_ID + "-b5",
                    subscribe=False,
                    secret="test-invalid",
                )
            except AAP2OperationFailed:
                pass
            else:
                assert False, "did not fail as expected"


@pytest.mark.skipif(not TEST_AAP2, reason="TEST_AAP2 disabled via environment")
def test_aap2_keepalive():
    sub_client = AAP2UnixClient(address=AAP2_SOCKET)
    with sub_client:
        sub_client.configure(
            AAP2_AGENT_ID + "-b6",
            subscribe=True,
            secret=AAP2_SECRET,
            keepalive_seconds=1,
        )
        # Test keepalive timing
        t1 = time.time()
        _wait_for_data(sub_client.socket)
        ka_msg = sub_client.receive_msg()
        t2 = time.time()
        assert ka_msg.WhichOneof("msg") == "keepalive"
        assert t1 + 1 + TIMING_MARGIN >= t2
        sub_client.send_response_status(ResponseStatus.RESPONSE_STATUS_ACK)
        t3 = time.time()
        _wait_for_data(sub_client.socket)
        ka_msg = sub_client.receive_msg()
        t4 = time.time()
        assert ka_msg.WhichOneof("msg") == "keepalive"
        assert t3 + 1 + TIMING_MARGIN >= t4
        # Test sending the wrong response
        sub_client.send_response_status(ResponseStatus.RESPONSE_STATUS_ERROR)
        _wait_until_connection_closed(sub_client.socket)
    # Test sending no response
    sub_client = AAP2UnixClient(address=AAP2_SOCKET)
    with sub_client:
        sub_client.configure(
            AAP2_AGENT_ID + "-b7",
            subscribe=True,
            secret=AAP2_SECRET,
            keepalive_seconds=1,
        )
        t5 = time.time()
        _wait_for_data(sub_client.socket)
        ka_msg = sub_client.receive_msg()
        t6 = time.time()
        assert ka_msg.WhichOneof("msg") == "keepalive"
        assert t5 + 1 + TIMING_MARGIN >= t6
        _wait_until_connection_closed(sub_client.socket)
        t7 = time.time()
        assert t6 + AAP2_AGENT_TIMEOUT + TIMING_MARGIN >= t7


@pytest.mark.skipif(not TEST_AAP2, reason="TEST_AAP2 disabled via environment")
def test_aap2_bundle_ack():
    rpc_client = AAP2UnixClient(address=AAP2_SOCKET)
    sub_client = AAP2UnixClient(address=AAP2_SOCKET)
    with rpc_client:
        with sub_client:
            s = rpc_client.configure(
                AAP2_AGENT_ID + "-b8",
                subscribe=False,
            )
            sub_client.configure(
                AAP2_AGENT_ID + "-b8",
                subscribe=True,
                secret=s,
            )
            payload = b"testbundle"
            rpc_client.send_adu(
                BundleADU(
                    dst_eid=rpc_client.eid,
                    payload_length=len(payload),
                ),
                payload,
            )
            response = rpc_client.receive_response()
            assert (response.response_status ==
                    ResponseStatus.RESPONSE_STATUS_SUCCESS)
            msg = sub_client.receive_msg()
            adu_msg, bundle_data = sub_client.receive_adu(msg.adu)
            # Check that reporting processing failure is ok and valid.
            sub_client.send_response_status(
                ResponseStatus.RESPONSE_STATUS_ERROR
            )
            # Send another ADU.
            rpc_client.send_adu(
                BundleADU(
                    dst_eid=rpc_client.eid,
                    payload_length=len(payload),
                ),
                payload,
            )
            response = rpc_client.receive_response()
            assert (response.response_status ==
                    ResponseStatus.RESPONSE_STATUS_SUCCESS)
            msg = sub_client.receive_msg()
            adu_msg, bundle_data = sub_client.receive_adu(msg.adu)
            # Do not acknowledge the ADU and check that the agent closes
            # the connection properly.
            t1 = time.time()
            _wait_until_connection_closed(sub_client.socket)
            t2 = time.time()
            assert t1 + AAP2_AGENT_TIMEOUT + TIMING_MARGIN >= t2
