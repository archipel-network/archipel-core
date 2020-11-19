#!/usr/bin/env python3
# encoding: utf-8
import socket
import sys

from pyd3tn.bundle7 import Bundle
from pyd3tn.mtcp import MTCPSocket


def main():
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-l", "--host",
        default="127.0.0.1",
        help="IP address to bind to (defaults to 127.0.0.1)",
    )
    parser.add_argument(
        "-p", "--port",
        type=int,
        default=42422,
        help="Port to bind to (defaults to 42422)",
    )
    parser.add_argument(
        "-b", "--bundle-version",
        default="7",
        choices="67",
        help="Version of the bundle protocol to use (defaults to 7): "
        "6 == RFC 5050, 7 == BPv7-bis"
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
    parser.add_argument(
        "--timeout",
        type=int, default=3000,
        help="TCP timeout in ms (default: 3000)"
    )

    args = parser.parse_args()

    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listener.bind((args.host, args.port))
    listener.listen(100)
    sock, addr = listener.accept()
    print("Accepted connection from {}".format(addr))
    listener.close()

    with MTCPSocket(sock, timeout=args.timeout) as conn:
        count = 0
        while True:
            data = conn.recv_bundle()
            if args.bundle_version == "7":
                data = Bundle.parse(data)
            print("Received bundle: {}".format(repr(data)))
            if (args.verify_pl is not None and args.bundle_version == "7" and
                    data.payload_block.data.decode("utf-8") != args.verify_pl):
                print("Unexpected payload != '{}'".format(args.verify_pl))
                sys.exit(1)
            count += 1
            if args.count and count >= args.count:
                break


if __name__ == "__main__":
    main()
