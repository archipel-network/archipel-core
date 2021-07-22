import pytest
import time

from cbor.cbor import loads

from ud3tn_utils.config import ConfigMessage, make_contact
from pyd3tn.bundle7 import serialize_bundle7, create_bundle7, BlockType
from pyd3tn.mtcp import MTCPConnection

from .helpers import (
    UD3TN_CONFIG_EP,
    UD3TN_HOST,
    SMTCP_PORT,
    TCP_TIMEOUT,
    TESTED_CLAS,
    validate_bundle7,
    send_delete_gs,
)

SENDING_GS_DEF = ("dtn://sender.dtn", "sender")
RECEIVING_GS_DEF = ("dtn://receiver.dtn", "receiver")

SENDING_CONTACT = (1, 1, 1000)
RECEIVING_CONTACT = (3, 1, 1000)
BUNDLE_SIZE = 200
PAYLOAD_DATA = b"\x42" * BUNDLE_SIZE


@pytest.mark.skipif("smtcp" not in TESTED_CLAS, reason="not selected")
def test_bundle_age():
    with MTCPConnection(UD3TN_HOST, SMTCP_PORT, timeout=TCP_TIMEOUT) as conn:
        outgoing_eid, outgoing_claaddr = SENDING_GS_DEF
        incoming_eid, incoming_claaddr = RECEIVING_GS_DEF

        # Configure contact during which we send a bundle
        conn.send_bundle(serialize_bundle7(
            outgoing_eid,
            UD3TN_CONFIG_EP,
            bytes(ConfigMessage(
                outgoing_eid,
                f"mtcp:{outgoing_claaddr}",
                contacts=[
                    make_contact(*SENDING_CONTACT),
                ],
            )),
        ))
        # Configure contact during which we want to receive the bundle
        conn.send_bundle(serialize_bundle7(
            outgoing_eid,
            UD3TN_CONFIG_EP,
            bytes(ConfigMessage(
                incoming_eid,
                f"smtcp:{incoming_claaddr}",
                contacts=[
                    make_contact(*RECEIVING_CONTACT),
                ],
            )),
        ))
        try:
            # Wait until first contact starts and send bundle
            time.sleep(SENDING_CONTACT[0])
            bundle = create_bundle7(
                outgoing_eid,
                incoming_eid,
                PAYLOAD_DATA,
                creation_timestamp=0,
                bundle_age=42,
            )
            start_time_s = time.time()
            conn.send_bundle(bytes(bundle))
            # Wait until second contact starts and try to receive bundle
            time.sleep(RECEIVING_CONTACT[0] - SENDING_CONTACT[0])
            bdl = conn.recv_bundle()
            duration_ms = (time.time() - start_time_s) * 1000
            bundle = validate_bundle7(bdl, PAYLOAD_DATA)
            age_block = next(filter(
                lambda b: b.block_type == BlockType.BUNDLE_AGE, bundle.blocks
            ))
            bundle_age = loads(age_block.data)
            assert(bundle_age >= 42000 and bundle_age <= (42000 + duration_ms))
            time.sleep(RECEIVING_CONTACT[1])
        finally:
            send_delete_gs(
                conn,
                serialize_bundle7,
                (outgoing_eid, incoming_eid)
            )
