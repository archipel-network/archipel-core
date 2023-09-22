#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
# encoding: utf-8

import argparse
import sys

import cbor  # type: ignore

from pyd3tn.bundle7 import Bundle
from ud3tn_utils.aap2 import (
    AAP2UnixClient,
    AAP2TCPClient,
    BundleADUFlags,
    ResponseStatus,
)
from helpers import add_common_parser_arguments, initialize_logger


def run_aap_recv(aap2_client, max_count, output, verify_pl, newline):
    logger.info("Waiting for bundles...")
    counter = 0

    while True:
        msg = aap2_client.receive_msg()
        if not msg:
            return

        if msg.WhichOneof("msg") != "adu":
            logger.info("Received message with field '%s' set, discarding.",
                        msg.WhichOneof("msg"))
            continue

        adu_msg, bundle_data = aap2_client.receive_adu(msg.adu)
        aap2_client.send_response_status(
            ResponseStatus.RESPONSE_STATUS_SUCCESS
        )

        enc = False
        err = False

        if (adu_msg.adu_flags & BundleADUFlags.BUNDLE_ADU_BPDU) != 0:
            payload = cbor.loads(bundle_data)
            bundle = Bundle.parse(payload[2])
            payload = bundle.payload_block.data
            enc = True
        else:
            payload = bundle_data

        if not err:
            enc = " encapsulated" if enc else ""
            logger.info(
                "Received%s bundle from '%s', payload len = %d",
                enc,
                msg.adu.src_eid,
                len(payload),
            )
            output.write(payload)
            if newline:
                output.write(b"\n")
            output.flush()
            if verify_pl is not None and verify_pl.encode("utf-8") != payload:
                logger.fatal("Unexpected payload != '%s'", verify_pl)
                sys.exit(1)
        else:
            logger.warning(
                "Received administrative record of unknown type from '%s'!",
                msg.adu.src_eid
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
    parser.add_argument(
        "--newline",
        action="store_true",
        help="print a line feed character after every received bundle payload",
    )

    args = parser.parse_args()
    logger = initialize_logger(args.verbosity)

    try:
        if args.tcp:
            aap2_client = AAP2TCPClient(address=args.tcp)
        else:
            aap2_client = AAP2UnixClient(address=args.socket)

        with aap2_client:
            secret = aap2_client.configure(
                args.agentid,
                subscribe=True,
                secret=args.secret,
            )
            logger.info("Assigned agent secret: '%s'", secret)
            run_aap_recv(
                aap2_client,
                args.count,
                args.output,
                args.verify_pl,
                args.newline,
            )
    finally:
        args.output.close()
