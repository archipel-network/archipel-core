#!/usr/bin/env python3
# encoding: utf-8

import argparse
import logging

from ud3tn_utils.aap import AAPUnixClient, AAPTCPClient
from helpers import add_common_parser_arguments, logging_level

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
        "payload",
        nargs="?",
        default=None,
        help="the payload of the created bundle, (default: read from STDIN)",
    )

    args = parser.parse_args()

    if not args.payload:
        with sys.stdin as f:
            args.payload = f.read()
    else:
        args.payload = args.payload

    if args.verbosity:
        logging.basicConfig(level=logging_level(args.verbosity))

    if args.tcp:
        addr = (args.tcp[0], int(args.tcp[1]))
        with AAPTCPClient(address=addr) as aap_client:
            aap_client.register(args.agentid)
            aap_client.send_str(args.dest_eid, args.payload)
    else:
        with AAPUnixClient(address=args.socket) as aap_client:
            aap_client.register(args.agentid)
            aap_client.send_str(args.dest_eid, args.payload)
