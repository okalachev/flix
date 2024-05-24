#!/usr/bin/env python3
# grab flight log and save to file

import datetime
import serial
import os

PORT = os.environ['PORT']
DIR = os.path.dirname(os.path.realpath(__file__))

dev = serial.Serial(port=PORT, baudrate=115200, timeout=0.5)
lines = []

print('Downloading log...')
count = 0
dev.write('log\n'.encode())
while True:
    line = dev.readline()
    if not line:
        break
    lines.append(line)
    count += 1
    print(f'\r{count} lines', end='')

# sort by timestamp
header = lines.pop(0)
lines.sort(key=lambda line: float(line.split(b',')[0]))

log = open(f'{DIR}/log/{datetime.datetime.now().isoformat()}.csv', 'wb')
log.writelines([header] + lines)
print(f'\nWritten {os.path.relpath(log.name, os.curdir)}')
