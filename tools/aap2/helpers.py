#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
# encoding: utf-8

import argparse
import logging
import sys

DEFAULT_CONFIG_AGENT_ID_DTN = "config"
DEFAULT_CONFIG_AGENT_ID_IPN = "9000"


def initialize_logger(verbosity: int):
    log_level = {
        0: logging.WARN,
        1: logging.INFO,
    }.get(verbosity, logging.DEBUG)
    logging.basicConfig(
        level=log_level,
        format="%(asctime)s %(levelname)s: %(message)s",
    )
    return logging.getLogger(sys.argv[0])


def add_common_parser_arguments(parser):
    add_agentid_parser_argument(parser)
    add_secret_parser_argument(parser)
    add_socket_group_parser_arguments(parser)
    add_verbosity_parser_argument(parser)


def add_agentid_parser_argument(parser):
    parser.add_argument(
        "-a", "--agentid",
        default=None,
        help="the agent id to register with uD3TN (default: random UUID)",
    )


def add_keepalive_parser_argument(parser):

    def _range_limited_keepalive(argument):
        try:
            keepalive = int(argument)
        except ValueError:
            raise argparse.ArgumentTypeError("keepalive value must be integer")
        if keepalive < 0 or keepalive > 86400:
            raise argparse.ArgumentTypeError(
                "invalid keepalive value, must be in range [0, 86400]"
            )
        return keepalive

    parser.add_argument(
        "-k", "--keepalive-seconds",
        type=_range_limited_keepalive,
        default=0,
        help=(
            "amount of seconds after which µD3TN should send a keepalive "
            "message to the receiving end (default: disabled)"
        ),
    )


def add_secret_parser_argument(parser):
    parser.add_argument(
        "-s", "--secret",
        default=None,
        help="the AAP secret to authenticate the endpoint (default: generate)",
    )


def add_socket_group_parser_arguments(parser):
    socket_group = parser.add_mutually_exclusive_group()
    socket_group.add_argument(
        "--socket",
        metavar="PATH",
        default="ud3tn.aap2.socket",
        type=str,
        help=(
            "AAP 2.0 UNIX domain socket to connect to "
            "(default: ud3tn.aap2.socket)"
        ),
    )
    socket_group.add_argument(
        "--tcp",
        nargs=2,
        metavar=("IP", "PORT"),
        type=str,
        help="AAP 2.0 TCP socket to connect to",
    )


def add_verbosity_parser_argument(parser):
    parser.add_argument(
        "-v", "--verbosity",
        action="count",
        default=0,
        help="increase output verbosity"
    )


def get_node_eid_prefix(eid):
    if eid[0:6] == "dtn://":
        return "dtn://" + eid.split("/")[2] + "/"
    elif eid[0:4] == "ipn:":
        return eid.split(".")[0] + "."
    else:
        raise ValueError("Cannot determine the node prefix for the given EID.")


def get_config_eid(eid):
    return get_node_eid_prefix(eid) + (
        DEFAULT_CONFIG_AGENT_ID_DTN
        if eid[0] == "d"  # get_node_eid_prefix already checks everything else
        else DEFAULT_CONFIG_AGENT_ID_IPN
    )
