#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
# encoding: utf-8

"""
A tool that reads a contact plan in ION format
and configures the uD3TN nodes.
"""

import argparse
from datetime import datetime
import sys

from ud3tn_utils.aap import AAPTCPClient, AAPUnixClient
from ud3tn_utils.config import ConfigMessage, unix2dtn, Contact

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
        if len(line) > 1:
            nodeInfo = line.split()
        else:
            continue
        if (int(nodeInfo[0]) < 1 or int(nodeInfo[0]) > 2 ** 64 - 1):
            raise Exception(
                "The given node number doesn't match the " +
                "required format. Should be: unsigned 64bit number; " +
                f"Was: {nodeInfo[0]}")

        if nodeInfo[2] == "tcp":
            nodes[nodeInfo[0]] = (nodeInfo[1], (nodeInfo[3], int(nodeInfo[4])))
        else:
            nodes[nodeInfo[0]] = (nodeInfo[1], nodeInfo[3])

    return nodes


def create_contact(start, end, bitrate):
    """Creates a `Contact` tuple for the ConfigMessage converts
    absolute or relative times into DTN timestamps

    Args:
        start (str): Start point of the contact
        end (str): End point of the contact
        bitrate (str): Bitrate of the contact
    Returns:
        Contact: tuple with DTN timestamps
    """
    if start.startswith("+"):
        start_time = unix2dtn(datetime.timestamp(datetime.now()) + int(start))
        end_time = unix2dtn(datetime.timestamp(datetime.now()) + int(end))
        return Contact(
            start=int(round(start_time)),
            end=int(round(end_time)),
            bitrate=int(bitrate),
        )
    else:
        start_time = datetime.strptime(start, "%Y/%m/%d-%H:%M:%S")
        end_time = datetime.strptime(end, "%Y/%m/%d-%H:%M:%S")

        if start_time < datetime.now() or end_time < datetime.now():
            print("Warning! The planned time for the contact has passed.")

        return Contact(
            start=int(round(unix2dtn(datetime.timestamp(start_time)))),
            end=int(round(unix2dtn(datetime.timestamp(end_time)))),
            bitrate=int(bitrate),
        )


# Converts node's name into ipn EID scheme format
def to_ipn(nodeNumber: str):
    return "ipn:" + str(nodeNumber) + ".0"


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
        if line.startswith("a contact"):
            planLine = line.split()
            if len(planLine) < 7:
                print(f"Error in contact plan on line {i + 1}:",
                      "Too few fields provided!")
                sys.exit(1)

            # exception when the nodes were not written in configuration file
            if planLine[4] not in nodes or planLine[5] not in nodes:
                raise Exception(f"Configuration of the nodes: {planLine[4]}",
                                f" or {planLine[5]} was not given")

            msg = bytes(ConfigMessage(
                to_ipn(planLine[5]),
                nodes[planLine[5]][0],
                contacts=[create_contact(planLine[2],
                                         planLine[3],
                                         planLine[6])],
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
                    dest_eid = to_ipn(planLine[4]) or aap_client.node_eid
                    aap_client.send_bundle(get_config_eid(dest_eid), msg)
            else:
                addr = nodes[planLine[4]][1]
                with AAPUnixClient(address=addr) as aap_client:
                    aap_client.register()
                    dest_eid = to_ipn(planLine[4]) or aap_client.node_eid
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
