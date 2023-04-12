#!/usr/bin/env python3
# grab flight log and save to file

import datetime
import serial
import os

PORT = os.environ['PORT']
DIR = os.path.dirname(os.path.realpath(__file__))

dev = serial.Serial(port=PORT, baudrate=115200, timeout=0.5)

log = open(f'{DIR}/log/{datetime.datetime.now().isoformat()}.csv', 'wb')

print('Downloading log...')
count = 0
dev.write('log\n'.encode())
while True:
    line = dev.readline()
    if not line:
        break
    log.write(line)
    count += 1
    print(f'\r{count} lines', end='')

print(f'\nWritten {os.path.relpath(log.name, os.curdir)}')
