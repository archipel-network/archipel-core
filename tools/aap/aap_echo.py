#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
# encoding: utf-8

"""
A tool that sends received bundles back to the sender.
"""

import sys
import argparse
import logging


from ud3tn_utils.aap import AAPUnixClient, AAPTCPClient, AAPMessage
from helpers import add_common_parser_arguments, logging_level
from ud3tn_utils.aap.aap_message import AAPMessageType


def run_echo(aap_client):

    print(
        f"Registered EID {aap_client.eid}"
        f"\nWaiting for bundles..."
    )

    while True:

        try:

            msg = aap_client.receive()
        except KeyboardInterrupt:

            print("Quitting")
            return

        if not msg:

            print("Lost connection, quitting")
            sys.exit(1)

        if msg.msg_type == AAPMessageType.RECVBUNDLE:

            print(
                f"-> Received bundle of length {len(msg.payload)} byte"
                f" from {msg.eid} "
            )
            aap_client.socket.send(
                AAPMessage(
                    AAPMessageType.SENDBUNDLE,
                    msg.eid,
                    msg.payload
                ).serialize()
            )
            print(
                f"-> Have sent a bundle back to {msg.eid}"
            )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="sends received bundles back to the sender",
    )

    add_common_parser_arguments(parser)

    args = parser.parse_args()

    if args.verbosity:
        logging.basicConfig(level=logging_level(args.verbosity))

    if args.tcp:
        addr = (args.tcp[0], int(args.tcp[1]))
        with AAPTCPClient(address=addr) as aap_client:
            aap_client.register(args.agentid)
            run_echo(aap_client)
    else:
        with AAPUnixClient(address=args.socket) as aap_client:
            aap_client.register(args.agentid)
            run_echo(aap_client)
