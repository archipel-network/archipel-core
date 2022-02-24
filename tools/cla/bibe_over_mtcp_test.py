#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
# encoding: utf-8

from pyd3tn.bundle7 import (
    BibeProtocolDataUnit,
    BundleProcFlag,
    PayloadBlock,
    PrimaryBlock,
    Bundle
)
from pyd3tn.mtcp import MTCPConnection

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
        default=4224,
        help="Port to connect to (defaults to 4224)",
    )
    parser.add_argument(
        "--payload",
        default=None,
        help="the payload to be sent"
    )
    parser.add_argument(
        "-o", "--outer",
        default="dtn://lower.dtn/bibe",
        help="EID to which the outer bundle should be adressed \
            (defaults to dtn://lower.dtn/bibe)",
    )
    parser.add_argument(
        "-i", "--inner",
        default="dtn://upper.dtn/bundlesink",
        help="EID to which the inner bundle should be adressed \
            (defaults to dtn://upper.dtn/bundlesink)",
    )
    parser.add_argument(
        "--timeout",
        type=int, default=3000,
        help="TCP timeout in ms (default: 3000)"
    )
    parser.add_argument(
        "--compatibility",
        action='store_true',
        help="If set switches BIBE administrative record type \
            code to 7 for compatibility with older implementations."
    )

    args = parser.parse_args()

    with MTCPConnection(args.host, args.port, timeout=args.timeout) as conn:
        payload = args.payload.encode() if args.payload else PAYLOAD_DATA
        compat = args.compatibility
        destination_eid = args.inner
        application_eid = args.outer
        outgoing_eid = "dtn://sender.dtn/"
        inner_bundle = Bundle(
            PrimaryBlock(
                destination=destination_eid,
                source=outgoing_eid
            ),
            PayloadBlock(payload)
        )
        encapsulating_bundle = Bundle(
            PrimaryBlock(
                bundle_proc_flags=BundleProcFlag.ADMINISTRATIVE_RECORD,
                destination=application_eid,
                source=outgoing_eid
            ),
            BibeProtocolDataUnit(
                bundle=inner_bundle,
                transmission_id=0,
                retransmission_time=0,
                compatibility=compat)
        )
        conn.send_bundle(bytes(encapsulating_bundle))


if __name__ == "__main__":
    main()
