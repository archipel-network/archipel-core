#!/usr/bin/env python3
# encoding: utf-8


def logging_level(verbosity):
    level = [
        0,   # logging.NOTSET
        50,  # logging.CRITICAL
        40,  # logging.ERROR
        30,  # logging.WARNING
        20,  # logging.INFO
        10,  # logging.DEBUG
    ]
    return level[verbosity] if verbosity <= 5 else level[5]


def add_common_parser_arguments(parser):
    add_socket_group_parser_arguments(parser)
    add_agentid_parser_argument(parser)
    add_verbosity_parser_argument(parser)


def add_verbosity_parser_argument(parser):
    parser.add_argument(
        "-v", "--verbosity",
        action="count",
        help="increase output verbosity"
    )


def add_agentid_parser_argument(parser):
    parser.add_argument(
        "-a", "--agentid",
        default=None,
        help="the agent id to register with uD3TN (default: random UUID)",
    )


def add_socket_group_parser_arguments(parser):
    socket_group = parser.add_mutually_exclusive_group()
    socket_group.add_argument(
        "--socket",
        metavar="PATH",
        default="ud3tn.socket",
        type=str,
        help=(
            "AAP UNIX domain socket to connect to "
            "(default: ud3tn.socket)"
        ),
    )
    socket_group.add_argument(
        "--tcp",
        nargs=2,
        metavar=("IP", "PORT"),
        type=str,
        help="AAP TCP socket to connect to",
    )
