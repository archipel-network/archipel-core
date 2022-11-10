#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
# encoding: utf-8

import argparse
import sys

import cbor  # type: ignore

from pyd3tn.bundle7 import Bundle
from ud3tn_utils.aap import AAPUnixClient, AAPTCPClient
from helpers import add_common_parser_arguments, initialize_logger
from ud3tn_utils.aap.aap_message import AAPMessageType


def run_aap_recv(aap_client, max_count, output, verify_pl):
    logger.info("Waiting for bundles...")
    counter = 0

    while True:
        msg = aap_client.receive()
        if not msg:
            return

        enc = False
        err = False
        if msg.msg_type == AAPMessageType.RECVBUNDLE:
            payload = msg.payload
        elif msg.msg_type == AAPMessageType.RECVBIBE:
            payload = cbor.loads(msg.payload)
            bundle = Bundle.parse(payload[2])
            payload = bundle.payload_block.data
            enc = True

        if not err:
            enc = " encapsulated" if enc else ""
            logger.info(
                "Received%s bundle from '%s', payload len = %d",
                enc,
                msg.eid,
                len(payload),
            )
            output.write(payload)
            output.flush()
            if verify_pl is not None and verify_pl.encode("utf-8") != payload:
                logger.fatal("Unexpected payload != '%s'", verify_pl)
                sys.exit(1)
        else:
            logger.warning(
                "Received administrative record of unknown type from '%s'!",
                msg.eid
            )

        counter += 1
        if max_count and counter >= max_count:
            logger.info("Expected amount of bundles received, terminating.")
            return


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="register an agent with uD3TN and wait for bundles",
    )

    add_common_parser_arguments(parser)

    parser.add_argument(
        "-c", "--count",
        type=int,
        default=None,
        help="amount of bundles to be received before terminating",
    )
    parser.add_argument(
        "-o", "--output",
        type=argparse.FileType("wb"),
        default=sys.stdout.buffer,
        help="file to write the received bundle contents",
    )
    parser.add_argument(
        "--verify-pl",
        default=None,
        help="verify that the payload is equal to the provided string",
    )

    args = parser.parse_args()
    logger = initialize_logger(args.verbosity)

    try:
        if args.tcp:
            with AAPTCPClient(address=args.tcp) as aap_client:
                aap_client.register(args.agentid)
                run_aap_recv(
                    aap_client,
                    args.count,
                    args.output,
                    args.verify_pl,
                )
        else:
            with AAPUnixClient(address=args.socket) as aap_client:
                aap_client.register(args.agentid)
                run_aap_recv(
                    aap_client,
                    args.count,
                    args.output,
                    args.verify_pl,
                )
    finally:
        args.output.close()
