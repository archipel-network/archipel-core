#!/usr/bin/env python3
# encoding: utf-8

import socket
from pyupcn.aap import AAPMessage, AAPMessageType, InsufficientAAPDataError

UPCN_AAP_ADDR = ("/tmp/upcn.socket")

AGENT_ID = "testagent"


def recv_aap(sock):
    buf = bytearray()
    msg = None
    while msg is None:
        buf += sock.recv(1)
        try:
            msg = AAPMessage.parse(buf)
        except InsufficientAAPDataError:
            continue
    return msg


def run_aap_test():

    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock:

        sock.connect(UPCN_AAP_ADDR)
        print("Connected to uPCN, awaiting WELCOME message...")

        msg_welcome = recv_aap(sock)
        assert msg_welcome.msg_type == AAPMessageType.WELCOME
        print("WELCOME message received! ~ EID = {}".format(msg_welcome.eid))
        base_eid = msg_welcome.eid

        print("Sending PING message...")
        sock.send(AAPMessage(AAPMessageType.PING).serialize())
        msg_ack = recv_aap(sock)
        assert msg_ack.msg_type == AAPMessageType.ACK
        print("ACK message received!")

        print("Sending REGISTER message...")
        sock.send(AAPMessage(AAPMessageType.REGISTER, AGENT_ID).serialize())
        msg_ack = recv_aap(sock)
        assert msg_ack.msg_type == AAPMessageType.ACK
        print("ACK message received!")
        my_eid = base_eid + "/" + AGENT_ID

        print("Sending PING message...")
        sock.send(AAPMessage(AAPMessageType.PING).serialize())
        msg_ack = recv_aap(sock)
        assert msg_ack.msg_type == AAPMessageType.ACK
        print("ACK message received!")

        print("Sending bundle to myself...")
        sock.send(AAPMessage(AAPMessageType.SENDBUNDLE,
                             my_eid, b"42").serialize())
        msg_sendconfirm = recv_aap(sock)
        assert msg_sendconfirm.msg_type == AAPMessageType.SENDCONFIRM
        print("SENDCONFIRM message received! ~ ID = {}".format(
            msg_sendconfirm.bundle_id
        ))

        msg_bdl = recv_aap(sock)
        assert msg_bdl.msg_type == AAPMessageType.RECVBUNDLE
        print("Bundle received from {}, payload = {}".format(
            msg_bdl.eid, msg_bdl.payload
        ))

        print("Terminating connection...")
        sock.shutdown(socket.SHUT_RDWR)


if __name__ == "__main__":
    run_aap_test()
