#!/usr/bin/env python3
# encoding: utf-8

import argparse
import logging

from ud3tn_utils.aap import AAPTCPClient, AAPUnixClient

from ud3tn_utils.config import ConfigMessage, make_contact


from helpers import (
    add_socket_group_parser_arguments,
    add_verbosity_parser_argument,
    logging_level,
)


if __name__ == "__main__":
    import sys

    parser = argparse.ArgumentParser(
        description="create or update a node in uD3TN",
    )

    add_socket_group_parser_arguments(parser)
    add_verbosity_parser_argument(parser)

    parser.add_argument(
        "--dest_eid",
        default=None,
        help="the EID of the node to which the configuration belongs",
    )
    parser.add_argument(
        "eid",
        help="the EID of the node to which the contact exists",
    )
    parser.add_argument(
        "cla_address",
        help="the CLA address of the node",
    )
    parser.add_argument(
        "-s", "--schedule",
        nargs=3,
        type=int,
        metavar=("start_offset", "duration", "bitrate"),
        action="append",
        default=[],
        help="schedule a contact relative to the current time",
    )
    parser.add_argument(
        "-r", "--reaches",
        type=str,
        action="append",
        default=[],
        help="specify an EID reachable via the node",
    )

    args = parser.parse_args()

    if args.verbosity:
        logging.basicConfig(level=logging_level(args.verbosity))

    if not args.schedule:
        print("at least one -s/--schedule argument must be given",
              file=sys.stderr)
        sys.exit(1)

    msg = bytes(ConfigMessage(
        args.eid,
        args.cla_address,
        contacts=[
            make_contact(*contact)
            for contact in args.schedule
        ],
        reachable_eids=args.reaches,
    ))

    print(msg)

    if args.tcp:
        addr = (args.tcp[0], int(args.tcp[1]))
        with AAPTCPClient(address=addr) as aap_client:
            aap_client.register()
            dest_eid = args.dest_eid or aap_client.node_eid
            aap_client.send_bundle(f"{dest_eid}/config", msg)
    else:
        with AAPUnixClient(address=args.socket) as aap_client:
            aap_client.register()
            dest_eid = args.dest_eid or aap_client.node_eid
            aap_client.send_bundle(f"{dest_eid}/config", msg)
