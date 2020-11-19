#!/usr/bin/env python3
# encoding: utf-8

import time

from ud3tn_utils.config import ConfigMessage, make_contact

from pyd3tn.bundle7 import serialize_bundle7, Bundle
from pyd3tn.bundle6 import serialize_bundle6
from pyd3tn.mtcp import MTCPConnection


SENDING_GS_DEF = ("dtn://sender.dtn", "sender")

SENDING_CONTACT = (1, 1, 1000)
RECEIVING_CONTACT = (3, 1, 1000)
BUNDLE_SIZE = 200
PAYLOAD_DATA = b"\x42" * BUNDLE_SIZE


def main():
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-l", "--host",
        default="127.0.0.1",
        help="IP address to connect to (defaults to 127.0.0.1)",
    )
    parser.add_argument(
        "-p", "--port",
        type=int,
        default=4222,
        help="Port to connect to (defaults to 4222)",
    )
    parser.add_argument(
        "-L", "--rx-host",
        default="127.0.0.1",
        help="IP address the receiver uses (defaults to 127.0.0.1)",
    )
    parser.add_argument(
        "-r", "--rx-port",
        type=int,
        default=42422,
        help="Port the receiver uses (defaults to 42422)",
    )
    parser.add_argument(
        "-t", "--type",
        choices=["smtcp", "mtcp", "usbotg"],
        default="smtcp",
        help="the CLA name and behavior to be used",
    )
    parser.add_argument(
        "-b", "--bundle-version",
        default="7",
        choices="67",
        help="Version of the bundle protocol to use (defaults to 7): "
        "6 == RFC 5050, 7 == BPv7-bis"
    )
    parser.add_argument(
        "-e", "--ud3tn-config-eid",
        default="dtn://ud3tn.dtn/config",
        help="EID of uD3TN's config endpoint"
    )
    parser.add_argument(
        "--payload",
        default=None,
        help="the payload to be sent"
    )
    parser.add_argument(
        "--timeout",
        type=int, default=3000,
        help="TCP timeout in ms (default: 3000)"
    )

    args = parser.parse_args()

    serialize_bundle = {
        "6": serialize_bundle6,
        "7": serialize_bundle7,
    }[args.bundle_version]

    with MTCPConnection(args.host, args.port, timeout=args.timeout) as conn:
        outgoing_eid, outgoing_claaddr = SENDING_GS_DEF
        incoming_eid = "dtn://receiver.dtn"
        incoming_claaddr_full = (
            args.type + ":" +
            str(args.rx_host) + ":" +
            str(args.rx_port)
        )
        # Configure contact during which we send a bundle
        conn.send_bundle(serialize_bundle(
            outgoing_eid,
            args.ud3tn_config_eid,
            bytes(ConfigMessage(
                outgoing_eid,
                args.type + ":" + outgoing_claaddr,
                contacts=[
                    make_contact(*SENDING_CONTACT),
                ],
            )),
        ))
        # Configure contact during which we want to receive the bundle
        conn.send_bundle(serialize_bundle(
            outgoing_eid,
            args.ud3tn_config_eid,
            bytes(ConfigMessage(
                incoming_eid,
                incoming_claaddr_full,
                contacts=[
                    make_contact(*RECEIVING_CONTACT),
                ],
            )),
        ))
        # Wait until first contact starts and send bundle
        time.sleep(SENDING_CONTACT[0])
        if args.payload:
            payload = args.payload.encode("utf-8")
        else:
            payload = PAYLOAD_DATA
        conn.send_bundle(serialize_bundle(
            outgoing_eid,
            incoming_eid,
            payload,
        ))
        # Wait until second contact starts and try to receive bundle
        if args.type != "mtcp":
            time.sleep(RECEIVING_CONTACT[0] - SENDING_CONTACT[0])
            data = conn.recv_bundle()
            if args.bundle_version == "7":
                bundle = Bundle.parse(data)
                print("Received bundle: {}".format(repr(bundle)))
            time.sleep(RECEIVING_CONTACT[1])


if __name__ == "__main__":
    main()
