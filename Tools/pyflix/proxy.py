#!/usr/bin/env python3

"""Proxy for running pyflix library alongside QGroundControl app."""

import socket

LOCAL = ('0.0.0.0', 14550)  # from Flix
TARGETS = (
    ('127.0.0.1', 14560),  # to QGroundControl
    ('127.0.0.1', 14555),  # to pyflix
)

def main():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(LOCAL)

    source_addr = None
    packets = 0

    print('Proxy started - run QGroundControl')

    while True:
        data, addr = sock.recvfrom(1024)  # read entire UDP packet
        if addr in TARGETS:  # packet from target
            if source_addr is None:
                continue
            try:
                sock.sendto(data, source_addr)
                packets += 1
            except: pass
        else:  # packet from source
            source_addr = addr
            for target in TARGETS:
                sock.sendto(data, target)
                packets += 1

        print(f'\rPackets: {packets}', end='')

if __name__ == '__main__':
    main()
