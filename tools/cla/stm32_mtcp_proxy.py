#!/usr/bin/env python3
# encoding: utf-8

"""Minimal tool to proxy usbotg (USB VCP) to MTCP"""

import argparse
import os
import sys
import time

import serial
import socket
import select
import termios


class DisconnectedError(Exception):
    pass


def _bind_socket(ip, port):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    sock.bind((ip, port))
    sock.listen()
    return sock


def _connect_serial(device):
    while True:
        try:
            usb = serial.Serial(
                device,
                timeout=0,
                exclusive=False,
            )
        except serial.serialutil.SerialException:
            time.sleep(0.5)
        else:
            break
    return usb


def _proxy_next_bytes(conn, usb, verbose):
    # Wait for both file descriptors to be ready to read from.
    # r will contain the file descriptors which are ready.
    r, _, _ = select.select([conn.fileno(), usb.fileno()], [], [])

    if usb.fileno() in r:
        from_usb = usb.read()
        if verbose:
            print("<< {}".format(
                "".join(["{:02x}".format(c) for c in from_usb])
            ))
        conn.send(from_usb)

    if conn.fileno() in r:
        from_sock = conn.recv(4096, socket.MSG_DONTWAIT)
        if not from_sock:
            raise DisconnectedError()
        if verbose:
            print(">> {}".format(
                "".join(["{:02x}".format(c) for c in from_sock])
            ))
        usb.write(from_sock)


def _main(args):
    sock = _bind_socket(args.ip, args.port)
    print("Listening on {}:{}".format(args.ip, args.port), file=sys.stderr)

    if not os.path.exists(args.device):
        print("WARNING: Device file not found, continuing anyway...",
              file=sys.stderr)

    sock_connected = False
    serial_connected = False

    while True:
        if not sock_connected:
            conn, addr = sock.accept()
            sock_connected = True
            print("Accepted TCP connection from: {}".format(addr),
                  file=sys.stderr)

        if not serial_connected:
            usb = _connect_serial(args.device)
            serial_connected = True
            print("Connected to serial port: {}".format(args.device),
                  file=sys.stderr)

        try:

            while True:
                _proxy_next_bytes(conn, usb, args.verbose)

        except (termios.error, serial.serialutil.SerialException):
            usb.close()
            print("Disconnected serial port.", file=sys.stderr)
            serial_connected = False

        except DisconnectedError:
            usb.close()
            print("Disconnected from TCP socket. Closing serial port as well.",
                  file=sys.stderr)
            sock_connected = False
            serial_connected = False


def _get_argument_parser():
    parser = argparse.ArgumentParser(
        description="tool to proxy uD3TN USB Virtual COM Port to MTCP"
    )
    parser.add_argument(
        "-d", "--device",
        default="/dev/ttyACM0",
        help="the USB serial device to use",
    )
    parser.add_argument(
        "--ip",
        default="127.0.0.1",
        help="IP to bind to (defaults to 127.0.0.1)",
    )
    parser.add_argument(
        "-p", "--port",
        type=int,
        default=4222,
        help="TCP port to bind to (defaults to 4222)",
    )
    parser.add_argument(
        "-v", "--verbose",
        action="store_true",
        help="show exchanged bytes in hexadecimal",
    )
    return parser


if __name__ == "__main__":
    _main(_get_argument_parser().parse_args())
