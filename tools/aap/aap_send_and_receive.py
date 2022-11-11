#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
# encoding: utf-8

"""Script to send a bundle and receive a specified number of bundles."""

import argparse
import cbor
import logging
import io
import sys

from pyd3tn.bundle7 import Bundle
from ud3tn_utils.aap import (
    AAPClient,
    AAPTCPClient,
    AAPUnixClient,
    AAPMessageType,
)

from helpers import add_common_parser_arguments, initialize_logger


def _send_and_recv(aap_client: AAPClient, dest_eid: str, payload: bytes,
                   count: int, output: io.BufferedIOBase,
                   logger: logging.Logger):
    logger.info("Sending payload of length %d to %s", len(payload), dest_eid)
    aap_client.send_bundle(dest_eid, payload, False)
    for i in range(count):
        logger.info("Waiting for reception of bundle #%d of %d", i + 1, count)
        while True:
            msg = aap_client.receive()
            if not msg:
                logger.warning("Received disconnect, terminating")
                return
            enc = False
            if msg.msg_type == AAPMessageType.RECVBUNDLE:
                payload = msg.payload
            elif msg.msg_type == AAPMessageType.RECVBIBE:
                payload = cbor.loads(msg.payload)
                bundle = Bundle.parse(payload[2])
                payload = bundle.payload_block.data
                enc = True
            else:
                continue
            enc_str = " encapsulated" if enc else ""
            logger.info(
                "Received%s bundle from '%s' with payload of len = %d",
                enc_str,
                msg.eid,
                len(payload),
            )
            output.write(payload)
            output.flush()
            break
    logger.info("Expected number of bundles received, terminating")


def _parse_args():
    parser = argparse.ArgumentParser(
        description="send a bundle via uD3TN and wait for responses",
    )
    add_common_parser_arguments(parser)
    parser.add_argument(
        "DESTINATION",
        help="the destination EID of the created bundle",
    )
    parser.add_argument(
        "PAYLOAD",
        type=argparse.FileType("rb"),
        default=sys.stdin.buffer,
        nargs="?",
        help="the payload of the created bundle, (default: read from STDIN)",
    )
    parser.add_argument(
        "-c", "--count",
        type=int,
        default=1,
        help="count of bundles expected to be received (default: 1)",
    )
    parser.add_argument(
        "-o", "--output",
        type=argparse.FileType("wb"),
        default=sys.stdout.buffer,
        help="file to write the received bundle contents",
    )
    return parser.parse_args()


if __name__ == "__main__":

    args = _parse_args()
    logger = initialize_logger(args.verbosity)

    payload = args.PAYLOAD.read()
    args.PAYLOAD.close()

    try:
        if args.tcp:
            aap_client = AAPTCPClient(address=args.tcp)
        else:
            aap_client = AAPUnixClient(address=args.socket)
        aap_client.connect()
        try:
            aap_client.register(args.agentid)
            _send_and_recv(
                aap_client,
                args.DESTINATION,
                payload,
                args.count,
                args.output,
                logger,
            )
        finally:
            aap_client.disconnect()
    finally:
        args.output.close()
