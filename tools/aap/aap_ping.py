#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
# encoding: utf-8

"""
A tool to send "ping bundles" (like the ICMP echos we all know and love)
to another DTN node, expect responses from there, and measure the time in
between.
"""

import argparse
import logging
import os
import signal
import sys
import time
import threading

from ud3tn_utils.aap import AAPUnixClient, AAPTCPClient, AAPMessage
from helpers import add_common_parser_arguments, logging_level
from ud3tn_utils.aap.aap_message import AAPMessageType


def _send_pings(aap_client, destination, interval, count, stop_event):

    counter = 0
    while counter < count or not count:
        # Send a bundle containing the current count & timestamp as a string.
        # Why the timestamp? -> To easily calculate how long it took to come
        # back! Otherwise, we would need some tricks for making the other
        # thread know when we sent which bundle.
        # We do not use aap_client.send but the lower-level function as
        # AAPClient invokes receive() at the end and this would confuse
        # the other thread if run here (we would receive partial data
        # everywhere).
        aap_client.socket.send(
            AAPMessage(
                AAPMessageType.SENDBUNDLE,
                destination,
                f"PING:{str(counter)}:{str(time.time())}".encode("utf-8"),
            ).serialize()
        )

        # We could use time.sleep() here, but this would not allow to properly
        # stop this thread. Thus, we rather wait until the stop Event occurs.
        # -> In normal operation this will not be the case and the function
        #    will run into the timeout in which case the condition is False.
        if stop_event.wait(timeout=interval):
            # We were requested to stop -> terminate by returning here
            return

        counter += 1

    # If we come here, termination has been requested after sending a
    # specified number of bundles and this was reached. We wait at maximum
    # for the interval to pass _again_ but do not send an additional bundle
    # during that period. Note that the receiver might trigger the stop event
    # if the last bundle has been received already.
    if stop_event.wait(timeout=interval):
        # We were requested to stop -> no need to signal the main thread
        return

    # Terminate the main thread via KeyboardInterrupt
    os.kill(os.getpid(), signal.SIGINT)


def _try_receive_ping(aap_client):
    # Wait for the next AAP message to be received
    msg = aap_client.receive()
    # Store the time we received the bundle as early as possible
    recv_time = time.time()

    # AAPClient.receive will return None, e.g., if uD3TN disconnects.
    if not msg:
        print("Lost connection, quitting")
        sys.exit(1)

    if msg.msg_type != AAPMessageType.RECVBUNDLE:
        # Nothing we want, continue loop.
        return False, None

    if msg.payload[0:4] != b"PING":
        # Just show we got something we do not want
        print(
            f"-> Received bundle of length {len(msg.payload)} byte "
            f"from {msg.eid} that does not seem to be a PING bundle!"
        )
        return False, None

    # Try to decode the payload of the message
    try:
        _, counter_str, time_str = msg.payload.decode("utf-8").split(":")
    except UnicodeError:  # if str.decode fails
        print(f"-> Could not decode PING bundle from {msg.eid}")
        return False, None

    # Try to get the numbers back
    try:
        counter_at_src = int(counter_str)
        time_at_src = float(time_str)
    except ValueError:  # int(...) or  float(...) failed
        print(f"-> Could not read values in PING bundle from {msg.eid}")
        return False, None

    # Calculate the Round Trip Time (duration from sending to receiving)
    rtt = recv_time - time_at_src
    print(f"Received PING from {msg.eid}: seq={counter_at_src}, rtt={rtt}")

    return True, counter_at_src


def run_aap_ping(aap_client, destination, interval, count):

    start_time = time.time()  # remember to calculate how many bdl. we expected

    # Start a second thread for sending the bundles!
    # Note that normally it is not a great idea to use threads in Python
    # due to the "Global Interpreter Lock" but in this case the threads mostly
    # wait for I/O (send/recv on the socket) which happens outside of the GIL.
    # Also note that the AAP client is not thread safe (it would call receive
    # on the socket twice concurrently and, thus, receive only partial garbage
    # in both threads). Hence, we directly use the socket send function
    # to make it thread safe...
    stop_event = threading.Event()  # for clean termination
    send_worker = threading.Thread(
        target=_send_pings,
        args=(aap_client, destination, interval, count, stop_event),
    )
    send_worker.start()

    receive_counter = 0
    try:
        while True:
            success, seqno = _try_receive_ping(aap_client)
            if success:
                receive_counter += 1
                # If we got the last bundle, terminate immediately
                if count and seqno == count - 1:
                    raise KeyboardInterrupt
    except KeyboardInterrupt:  # Ctrl+C was pressed or the sender signaled us
        # Calculate and print some statistics
        duration = time.time() - start_time
        expected_bundles = int(duration / interval) + 1
        if count:
            expected_bundles = min(expected_bundles, count)
        print(
            f"Ping ran for {duration}, received {receive_counter} of "
            f"{expected_bundles} sent"
        )
        if receive_counter < expected_bundles:
            # Indicate that we did not receive everything
            sys.exit(1)
    finally:  # Note that _try_receive_ping might raise SystemExit
        # Tell the sending thread to terminate
        stop_event.set()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description=(
            "send bundles to a given EID, wait for responses, "
            "and print them"
        ),
    )

    add_common_parser_arguments(parser)

    parser.add_argument(
        "destination",
        help="the destination EID to ping",
    )
    parser.add_argument(
        "-i", "--interval",
        type=float,
        default=1.0,
        help=(
            "interval, in seconds, to wait between sending bundles "
            "(default: 1 second)"
        ),
    )
    parser.add_argument(
        "-c", "--count",
        type=int,
        default=0,
        help=(
            "stop after sending the specified number of bundles "
            "(default: 0 = infinite)"
        ),
    )

    args = parser.parse_args()

    if args.verbosity:
        logging.basicConfig(level=logging_level(args.verbosity))

    if args.tcp:
        addr = (args.tcp[0], int(args.tcp[1]))
        with AAPTCPClient(address=addr) as aap_client:
            aap_client.register(args.agentid)
            run_aap_ping(aap_client, args.destination, args.interval,
                         args.count)
    else:
        with AAPUnixClient(address=args.socket) as aap_client:
            aap_client.register(args.agentid)
            run_aap_ping(aap_client, args.destination, args.interval,
                         args.count)
