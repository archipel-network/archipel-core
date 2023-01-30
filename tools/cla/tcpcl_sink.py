#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
# encoding: utf-8
import logging
import signal
import asyncio
from pyd3tn.tcpcl import TCPCLServer, logger


DEFAULT_INCOMING_EID = "dtn://1/"
BIND_TO = ("127.0.0.1", 42420)


async def listen():
    loop = asyncio.get_event_loop()
    async with TCPCLServer(DEFAULT_INCOMING_EID, *BIND_TO) as sink:
        loop.add_signal_handler(signal.SIGINT, sink.close)
        await sink.wait_closed()
        loop.remove_signal_handler(signal.SIGINT)


# Enable logging on stdout
logger.setLevel(logging.DEBUG)
console = logging.StreamHandler()
console.setLevel(logger.level)
logger.addHandler(console)

# Bootstrap event loop
asyncio.run(listen())
