#!/usr/bin/env python3
# encoding: utf-8

import argparse
import logging
import sys

from pyd3tn.bundle7 import BibeProtocolDataUnit, Bundle
from ud3tn_utils.aap import AAPUnixClient, AAPTCPClient
from helpers import add_common_parser_arguments, logging_level


def run_aap_recv(aap_client, max_count=None, verify_pl=None):
    print("Waiting for bundles...")
    counter = 0
    
    while True:
        msg = aap_client.receive()
        if not msg:
            return
        
        enc = False

        # Simple check if an encapsulated bundle was sent:
        # If the payload of msg is not a string but a
        # bundle, decode() will throw a UnicodeDecodeError.
        # -> in this case bundle has to be parsed to access
        # the BIBE AR, which in turn has to be parsed to 
        # access the payload. 
        try:
            payload = msg.payload.decode("utf-8")
        except UnicodeDecodeError:
            outer_bundle = Bundle.parse(msg.payload)
            bpdu = BibeProtocolDataUnit.parse_bibe_pdu(outer_bundle.payload_block.data)
            payload = Bundle.parse(bpdu["encapsulated_bundle"]).payload_block.data.decode("utf-8")
            enc = True

        enc = " encapsulated" if enc else ""
        print("Received{} bundle from '{}': {}".format(
            enc,
            msg.eid,
            payload,
        ))
        if verify_pl is not None and verify_pl != payload:
            print("Unexpected payload != '{}'".format(verify_pl))
            sys.exit(1)
        counter += 1
        if max_count and counter >= max_count:
            print("Expected amount of bundles received, terminating.")
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
        "--verify-pl",
        default=None,
        help="verify that the payload is equal to the provided string",
    )

    args = parser.parse_args()

    if args.verbosity:
        logging.basicConfig(level=logging_level(args.verbosity))

    if args.tcp:
        addr = (args.tcp[0], int(args.tcp[1]))
        with AAPTCPClient(address=addr) as aap_client:
            aap_client.register(args.agentid)
            run_aap_recv(aap_client, args.count, args.verify_pl)
    else:
        with AAPUnixClient(address=args.socket) as aap_client:
            aap_client.register(args.agentid)
            run_aap_recv(aap_client, args.count, args.verify_pl)
