#!/usr/bin/env python3
# encoding: utf-8

import argparse
import socket
import uuid
import sys

from pyupcn.aap import AAPMessage, AAPMessageType, InsufficientAAPDataError


def recv_aap(sock):
    buf = bytearray()
    msg = None
    while msg is None:
        data = sock.recv(1)
        if not data:
            # Connection lost
            return None
        buf += data
        try:
            msg = AAPMessage.parse(buf)
        except InsufficientAAPDataError:
            continue
    return msg


def run_aap_recv(sock, eid_suffix=None, max_count=None, verify_pl=None):
    eid_suffix = eid_suffix or str(uuid.uuid4())

    print("Connected to uPCN, awaiting WELCOME message...")

    msg_welcome = recv_aap(sock)
    assert msg_welcome.msg_type == AAPMessageType.WELCOME
    print("WELCOME message received! ~ EID = {}".format(msg_welcome.eid))

    print("Sending REGISTER message for '{}'...".format(eid_suffix))
    sock.send(AAPMessage(AAPMessageType.REGISTER, eid_suffix).serialize())
    msg_ack = recv_aap(sock)
    assert msg_ack.msg_type == AAPMessageType.ACK
    print("ACK message received!")

    print("Waiting for bundles...")
    try:
        counter = 0
        while True:
            msg = recv_aap(sock)
            if not msg:
                print("Disconnected.")
                return
            assert msg.msg_type == AAPMessageType.RECVBUNDLE
            payload = msg.payload.decode("utf-8")
            print("Received bundle from '{}': {}".format(
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
    finally:
        print("Terminating connection...")
        sock.shutdown(socket.SHUT_RDWR)
        sock.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="register an agent with uPCN and wait for bundles",
    )
    socket_group = parser.add_mutually_exclusive_group()
    socket_group.add_argument(
        "--socket",
        metavar="PATH",
        default="/tmp/upcn.socket",
        type=str,
        help=(
            "AAP UNIX domain socket to connect to "
            "(default: /tmp/upcn.socket)"
        ),
    )
    socket_group.add_argument(
        "--tcp",
        nargs=2,
        metavar=("IP", "PORT"),
        type=str,
        help="AAP TCP socket to connect to",
    )
    parser.add_argument(
        "-a", "--agentid",
        default=None,
        help="the agent id to register with uPCN (default: random UUID)",
    )
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

    if args.tcp:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            sock.connect((args.tcp[0], int(args.tcp[1])))
            run_aap_recv(
                sock,
                args.agentid,
                args.count,
                args.verify_pl,
            )
    else:
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock:
            sock.connect(args.socket)
            run_aap_recv(
                sock,
                args.agentid,
                args.count,
                args.verify_pl,
            )
