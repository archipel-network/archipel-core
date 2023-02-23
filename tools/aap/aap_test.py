#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
# encoding: utf-8

import argparse

from ud3tn_utils.aap import AAPUnixClient, AAPTCPClient
from helpers import add_socket_group_parser_arguments

AGENT_ID = "testagent"


def run_aap_test(aap_client):
    print("Sending PING message...")
    aap_client.ping()

    print("Sending REGISTER message...")
    aap_client.register(AGENT_ID)

    print("Sending PING message...")
    aap_client.ping()

    print(f"Sending bundle to myself ({aap_client.eid})...")
    aap_client.send_str(
        aap_client.eid,
        "42"
    )

    msg_bdl = aap_client.receive()
    print("Bundle received from {}, payload = {}".format(
        msg_bdl.eid, msg_bdl.payload
    ))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="run a very simple test for AAP connectivity",
    )
    add_socket_group_parser_arguments(parser)
    args = parser.parse_args()

    if args.tcp:
        with AAPTCPClient(address=args.tcp) as aap_client:
            run_aap_test(aap_client)
    else:
        with AAPUnixClient(address=args.socket) as aap_client:
            run_aap_test(aap_client)
