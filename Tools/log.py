#!/usr/bin/env python3

# Download flight log remotely and save to file

import os
import time
import datetime
import struct
from pymavlink.dialects.v20.common import MAVLink_log_data_message
from pyflix import Flix

DIR = os.path.dirname(os.path.realpath(__file__))
flix = Flix()

print('Downloading log...')

header = flix.cli('log')
print('Received header:\n- ' + '\n- '.join(header.split(',')))

records = []

def on_record(msg: MAVLink_log_data_message):
    global stop
    stop = time.time() + 1  # extend timeout
    records.append([])
    i = 0
    data = bytes(msg.data)
    while i + 4 <= msg.count:
        records[-1].append(struct.unpack('<f', data[i:i+4])[0])
        i += 4

stop = time.time() + 3
flix.on('mavlink.LOG_DATA', on_record)
flix.mavlink.log_request_data_send(flix.system_id, 0, 0, 0, 0xFFFFFFFF)

while time.time() < stop:
    time.sleep(1)

flix.off(on_record)

records.sort(key=lambda record: record[0])
records = [record for record in records if record[0] != 0]

print(f'Received records: {len(records)}')

os.makedirs(f'{DIR}/log', exist_ok=True)
log = open(f'{DIR}/log/{datetime.datetime.now().isoformat()}.csv', 'wb')
log.write(header.encode() + b'\n')
for record in records:
    line = ','.join(f'{value}' for value in record)
    log.write(line.encode() + b'\n')
print(f'Written {os.path.relpath(log.name, os.curdir)}')
