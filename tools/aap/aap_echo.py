#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
# encoding: utf-8

"""
A tool that sends received bundles back to the sender.
"""

import sys
import argparse

from ud3tn_utils.aap import AAPUnixClient, AAPTCPClient, AAPMessage
from helpers import add_common_parser_arguments, initialize_logger
from ud3tn_utils.aap.aap_message import AAPMessageType


def run_echo(aap_client):

    logger.info("Registered EID '%s', waiting for bundles...", aap_client.eid)

    while True:

        try:
            msg = aap_client.receive()
        except KeyboardInterrupt:
            logger.info("Quitting")
            return

        if not msg:
            logger.warning("Lost connection, quitting")
            sys.exit(1)

        if msg.msg_type == AAPMessageType.RECVBUNDLE:
            logger.debug(
                "Received bundle of length %d bytes from '%s'",
                len(msg.payload),
                msg.eid,
            )
            aap_client.socket.send(
                AAPMessage(
                    AAPMessageType.SENDBUNDLE,
                    msg.eid,
                    msg.payload
                ).serialize()
            )
            logger.debug("Sent a bundle back to '%s'", msg.eid)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="sends received bundles back to the sender",
    )

    add_common_parser_arguments(parser)

    args = parser.parse_args()
    logger = initialize_logger(args.verbosity + 1)  # log INFO by default

    if args.tcp:
        addr = (args.tcp[0], int(args.tcp[1]))
        with AAPTCPClient(address=addr) as aap_client:
            aap_client.register(args.agentid)
            run_echo(aap_client)
    else:
        with AAPUnixClient(address=args.socket) as aap_client:
            aap_client.register(args.agentid)
            run_echo(aap_client)
