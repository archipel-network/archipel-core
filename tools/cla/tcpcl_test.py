#!/usr/bin/env python3
# encoding: utf-8

import time
import socket
import argparse

from ud3tn_utils.config import ConfigMessage, make_contact

from pyd3tn.tcpcl import (
    serialize_tcpcl_contact_header,
    decode_tcpcl_contact_header,
    serialize_tcpcl_single_bundle_segment,
)
from pyd3tn.bundle6 import serialize_bundle6
from pyd3tn.bundle7 import serialize_bundle7

DEFAULT_OUTGOING_EID = "dtn:2"
DEFAULT_INCOMING_EID = "dtn:1"
DEFAULT_OWN_CLA_ADDRESS = "tcpclv3:127.0.0.1:42421"
# we have to listen on this port to properly test bundle transmission
DEFAULT_CONTACT_CLA_ADDRESS = "tcpclv3:127.0.0.1:42420"


def run_simple_forwarding_test(serialize_bundle, concurrent=False):

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.connect(("localhost", 4556))
        print("Connected to uD3TN, sending TCPCL header...")
        sock.sendall(serialize_tcpcl_contact_header(DEFAULT_OUTGOING_EID))
        ud3tn_contact_header = sock.recv(1024)
        header_decoded = decode_tcpcl_contact_header(ud3tn_contact_header)
        print("Received TCPCL header:", header_decoded)

        config_msg1 = ConfigMessage(
            DEFAULT_OUTGOING_EID,
            DEFAULT_OWN_CLA_ADDRESS,
            contacts=[
                make_contact(2, 8 if concurrent else 2, 500),
            ],
        )
        config_msg2 = ConfigMessage(
            DEFAULT_INCOMING_EID,
            DEFAULT_CONTACT_CLA_ADDRESS,
            contacts=[
                make_contact(3 if concurrent else 5, 10, 500),
            ],
        )
        print("Configuring contact:", config_msg1)
        sock.sendall(serialize_tcpcl_single_bundle_segment(
            serialize_bundle(
                DEFAULT_OUTGOING_EID,
                header_decoded["eid"] + "/config",
                bytes(config_msg1),
            )
        ))
        print("Configuring contact:", config_msg2)
        sock.sendall(serialize_tcpcl_single_bundle_segment(
            serialize_bundle(
                DEFAULT_OUTGOING_EID,
                header_decoded["eid"] + "/config",
                bytes(config_msg2),
            )
        ))

        print("Terminating TCPCL connection...")
        sock.sendall(b"\x50")
        sock.shutdown(socket.SHUT_RDWR)

    # wait for contact to start, connect and send bundle
    time.sleep(2.2)

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.connect(("localhost", 4556))
        print("Connected to uD3TN, sending TCPCL header...")
        sock.sendall(serialize_tcpcl_contact_header(DEFAULT_OUTGOING_EID))
        ud3tn_contact_header = sock.recv(1024)
        print(
            "Received TCPCL header:",
            decode_tcpcl_contact_header(ud3tn_contact_header),
        )

        # wait until uD3TN has processed the command and send the bundle
        sock.sendall(serialize_tcpcl_single_bundle_segment(
            serialize_bundle(
                DEFAULT_OUTGOING_EID,
                DEFAULT_INCOMING_EID,  # bundle to the configured EID
                b"\x42" * 42,
            )
        ))
        time.sleep(1.8)

        print("Terminating TCPCL connection...")
        sock.sendall(b"\x50")
        sock.shutdown(socket.SHUT_RDWR)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-p", "--protocol",
        choices=["6", "7"], default="6",
        help="the bundle protocol version to be used (defaults to 6)",
    )
    parser.add_argument(
        "--concurrent",
        action="store_true",
        help="use two concurrent contacts for sending/receiving",
    )
    args = parser.parse_args()
    serialize_bundle = {
        "6": serialize_bundle6,
        "7": serialize_bundle7,
    }.get(args.protocol)
    run_simple_forwarding_test(serialize_bundle, args.concurrent)
