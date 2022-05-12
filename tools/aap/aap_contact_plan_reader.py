#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
# encoding: utf-8

"""
A tool that reads a contact plan in ION format
and configures the uD3TN nodes.
"""

import argparse
from datetime import datetime

from ud3tn_utils.aap import AAPTCPClient, AAPUnixClient
from ud3tn_utils.config import ConfigMessage, make_contact

from helpers import get_config_eid


def get_nodes_data(nodesData):
    """Reads text file with nodes' addresses and adds them to dictionary

    Args:
        nodesData: Path to nodes configuration .txt-file
    Returns:
        nodes (dict): dictionary with each nodes and its addresses
    """
    textReader = open(nodesData, "r")
    nodesData = textReader.readlines()
    textReader.close()

    # dictionary in format name of the node : (cla-adress, (ip, port)/socket)
    # if aap connects via tcp then (cla-address, (ip, port))
    # if aap connects via socket then (cla-address, socketpath)
    nodes = {}

    for line in nodesData:
        # skipping empty lines
        if(len(line) > 1):
            nodeInfo = line.split()
        else:
            continue

        if nodeInfo[2] == "tcp":
            nodes[nodeInfo[0]] = (nodeInfo[1], (nodeInfo[3], int(nodeInfo[4])))
        else:
            nodes[nodeInfo[0]] = (nodeInfo[1], nodeInfo[3])

    return nodes


def parse_contact_time_span(start_time: str, end_time: str):
    """Parse contact time span dependent on time input format

    Args:
        start_time (str): Start time of the contact
        end_time (str): End time of the contact
    Returns: list [[t1, t2]]: Relative start and end times in seconds from now
    """
    if start_time[0] == "+":
        return [[int(start_time), int(end_time)]]
    else:
        start_time = datetime.strptime(start_time, "%Y/%m/%d-%H:%M:%S")
        end_time = datetime.strptime(end_time, "%Y/%m/%d-%H:%M:%S")

        if ((start_time - datetime.now()).total_seconds() < 0 or
           (end_time - datetime.now()).total_seconds() < 0):
            print("Warning! The scheduled time for contact is passed!")

        # here it transforms the absolute time into relative time to exact time
        # on device by subtracting the actual time on device from the absolute
        # start time and from end time accordingly
        return [[(start_time - datetime.now()).total_seconds(),
                (end_time - datetime.now()).total_seconds()]]


# Converts node's name into uD3TN format
def to_dtn(nodeName: str):
    return "dtn://" + str(nodeName) + ".dtn/"


def configure_nodes(contactPlan, nodes: dict):
    """Configure the nodes by scheduling the contacts

    Args:
        contactPlan: Path to contact plan .txt-file
        nodes (dict): dictionary of nodes and their addresses
    """
    textReader = open(contactPlan, "r")
    contactPlan = textReader.readlines()
    textReader.close()

    for i, line in enumerate(contactPlan):

        # processing only a contact command
        if (line.startswith("a contact")):
            planLine = line.split()

            # exception when the nodes were not written in configuration file
            if (planLine[4] not in nodes or planLine[5] not in nodes):
                raise Exception(f"Configuration of the nodes: {planLine[4]}",
                                f" or {planLine[5]} was not given")

            schedule = parse_contact_time_span(planLine[2], planLine[3])

            # adding data rate to schedule
            schedule[0].append(int(planLine[6]))

            # config message with node's eid and its cla address as parameters
            msg = bytes(ConfigMessage(
                to_dtn(planLine[5]),
                nodes[planLine[5]][0],
                contacts=[
                    make_contact(*contact)
                    for contact in schedule
                ],
            ))

            print(
                f"Establishing contact from {planLine[4]} to -> {planLine[5]}",
                f"Start time: {planLine[2]} end time: {planLine[3]}",
                f"With transmit rate: {planLine[6]} (bytes/s)"
                )

            # checking the type of aap port tcp/socket
            if type(nodes[planLine[4]][1]) == tuple:
                addr = nodes[planLine[4]][1]
                with AAPTCPClient(address=addr) as aap_client:
                    aap_client.register()
                    dest_eid = to_dtn(planLine[4]) or aap_client.node_eid
                    aap_client.send_bundle(get_config_eid(dest_eid), msg)
            else:
                addr = nodes[planLine[4]][1]
                with AAPUnixClient(address=addr) as aap_client:
                    aap_client.register()
                    dest_eid = to_dtn(planLine[4]) or aap_client.node_eid
                    aap_client.send_bundle(get_config_eid(dest_eid), msg)

        else:
            # skipping empty lines and comments
            if line == '\n' or line.startswith('#'):
                continue
            else:
                print(f"Skipping unsupported command in line {i + 1}")
                continue


if __name__ == "__main__":

    parser = argparse.ArgumentParser(
        description="reads the text file",
    )

    parser.add_argument(
        "--plan",
        default=None,
        help="the path to the contact plan as txt file",
    )

    parser.add_argument(
        "--nodes",
        default=None,
        help="the path to the txt file with information about nodes"
    )

    args = parser.parse_args()
    configure_nodes(args.plan, get_nodes_data(args.nodes))
