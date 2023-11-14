#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
# encoding: utf-8

import argparse
import cbor

from ud3tn_utils.aap2 import (
    AAP2TCPClient,
    AAP2UnixClient,
    BundleADU,
    BundleADUFlags,
    ResponseStatus,
)
from helpers import add_common_parser_arguments, initialize_logger
from pyd3tn.bundle7 import (
    BibeProtocolDataUnit,
    Bundle,
    PayloadBlock,
    PrimaryBlock,
)


def build_bpdu(inner_source, inner_dest, payload):
    """Encapsulates a regular bundle with the chosen payload
    in a BIBE Administrative Record, thus forming a BIBE Bundle.

    Args:
        inner_source (str): Source EID of the encapsulated bundle
        inner_dest (str): Destination EID of the encapsulated bundle
        payload (bytes): The payload of the encapsulated bundle

    Returns:
        BPDU (bytes): The bytes making up the BPDU
    """
    inner_bundle = Bundle(
        PrimaryBlock(
            destination=inner_dest,
            source=inner_source,
        ),
        PayloadBlock(payload)
    )
    bibe_ar = BibeProtocolDataUnit(
        bundle=inner_bundle,
        transmission_id=0,
        retransmission_time=0,
        compatibility=False,
    )

    return cbor.dumps(bibe_ar.record_data)


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
        "--bibe-source",
        default=None,
        help=("if set, the payload will be encapsulated twice using BIBE, "
              "with the source of the inner bundle set to the given EID"))
    parser.add_argument(
        "--bibe-destination",
        default=None,
        help=("if set, the payload will be encapsulated twice using BIBE, "
              "with the inner bundle addressed to the given EID"))
    args = parser.parse_args()

    if args.PAYLOAD:
        payload = args.PAYLOAD.encode("utf-8")
    else:
        payload = sys.stdin.buffer.read()
        sys.stdin.buffer.close()

    logger = initialize_logger(args.verbosity)

    is_bibe = (
        args.bibe_source is not None or
        args.bibe_destination is not None
    )
    if is_bibe and (args.bibe_source is None or args.bibe_destination is None):
        logger.fatal("--bibe-source and --bibe-destination must both be set")
        sys.exit(1)

    if args.tcp:
        aap2_client = AAP2TCPClient(address=args.tcp)
    else:
        aap2_client = AAP2UnixClient(address=args.socket)
    with aap2_client:
        secret = aap2_client.configure(
            args.agentid,
            subscribe=False,
            secret=args.secret,
        )
        logger.info("Assigned agent secret: '%s'", secret)
        if is_bibe:
            payload = build_bpdu(
                args.bibe_source,
                args.bibe_destination,
                payload,
            )
            flags = BundleADUFlags.BUNDLE_ADU_BPDU
        else:
            flags = BundleADUFlags.BUNDLE_ADU_NORMAL
        aap2_client.send_adu(
            BundleADU(
                dst_eid=args.dest_eid,
                payload_length=len(payload),
                adu_flags=flags,
            ),
            payload,
        )
        assert (
            aap2_client.receive_response().response_status ==
            ResponseStatus.RESPONSE_STATUS_SUCCESS
        )
