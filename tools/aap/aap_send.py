#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
# encoding: utf-8

import argparse

from ud3tn_utils.aap import AAPTCPClient, AAPUnixClient
from helpers import add_common_parser_arguments, initialize_logger
from pyd3tn.bundle7 import (BibeProtocolDataUnit, Bundle, BundleProcFlag,
                            PayloadBlock, PrimaryBlock)


def build_bibe_bundle(client, dest_eid, payload):
    """Encapsulates a regular bundle with the chosen payload
    in a BIBE Administrative Record, thus forming a BIBE Bundle.

    Args:
        client (AAPClient): The AAP client used to send the bundle
        dest_eid (str): EID of the bundles destination
        payload (bytes): The payload of the encapsulated bundle

    Returns:
        Bundle: A bundle containing the BIBE Administrative record
    """
    inner_bundle = Bundle(
        PrimaryBlock(
            destination=dest_eid,
            source=client.eid
        ),
        PayloadBlock(payload)
    )
    outer_bundle = Bundle(
        PrimaryBlock(
            bundle_proc_flags=BundleProcFlag.ADMINISTRATIVE_RECORD,
            destination=dest_eid,
            source=client.eid
        ),
        BibeProtocolDataUnit(
            bundle=inner_bundle,
            transmission_id=0,
            retransmission_time=0,
            compatibility=False))

    return outer_bundle


if __name__ == "__main__":
    import sys

    parser = argparse.ArgumentParser(
        description="send a bundle via uD3TN's AAP interface",
    )
    add_common_parser_arguments(parser)
    parser.add_argument(
        "dest_eid",
        help="the destination EID of the created bundle",
    )
    parser.add_argument(
        "PAYLOAD",
        default=None,
        nargs="?",
        help="the payload of the created bundle, (default: read from STDIN)",
    )
    parser.add_argument(
        '--bibe',
        action='store_true',
        help="if set, bundle will be encapsulated before sending")
    args = parser.parse_args()

    if args.PAYLOAD:
        payload = args.PAYLOAD.encode("utf-8")
    else:
        payload = sys.stdin.buffer.read()
        sys.stdin.buffer.close()

    logger = initialize_logger(args.verbosity)

    if args.tcp:
        with AAPTCPClient(address=args.tcp) as aap_client:
            if not args.bibe:
                aap_client.register(args.agentid)
                aap_client.send_bundle(args.dest_eid, payload, False)
            else:
                aap_client.register(args.agentid)
                encapsulating_bundle = build_bibe_bundle(
                    aap_client,
                    args.dest_eid,
                    payload,
                )
                aap_client.send_bundle(
                    args.dest_eid,
                    bytes(encapsulating_bundle),
                    args.bibe)
    else:
        with AAPUnixClient(address=args.socket) as aap_client:
            if not args.bibe:
                aap_client.register(args.agentid)
                aap_client.send_bundle(args.dest_eid, payload, False)
            else:
                aap_client.register(args.agentid)
                encapsulating_bundle = build_bibe_bundle(
                    aap_client,
                    args.dest_eid,
                    payload,
                )
                aap_client.send_bundle(
                    args.dest_eid,
                    bytes(encapsulating_bundle),
                    args.bibe)
