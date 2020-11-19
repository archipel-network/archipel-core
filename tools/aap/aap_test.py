#!/usr/bin/env python3
# encoding: utf-8

from ud3tn_utils.aap import AAPUnixClient

AGENT_ID = "testagent"


def run_aap_test():

    with AAPUnixClient() as aap_client:
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
    run_aap_test()
