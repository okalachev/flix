#!/usr/bin/env python3

"""Convert flight from Flix format to CSV

Usage:
    log_to_csv.py <input_file>
"""

import os
from pyflix import Flix
import docopt
import struct
import csv

DIR = os.path.dirname(os.path.realpath(__file__))
HEADER_FILE = os.path.join(DIR, 'log/log_header.txt')

# Read log header
try:
    # read from file
    header = open(HEADER_FILE, 'r').read()
except FileNotFoundError:
    flix = Flix()
    header = flix.cli('log header')  # receive the log schema
    open(HEADER_FILE, 'w').write(header)  # save to file

# Parse log header
topics = []
for line in header.splitlines():
    if not line.startswith('  '):
        topics.append([])
    elif 'not logged' not in line:
        topics[-1].append(line.strip())

# Read log file
args = docopt.docopt(__doc__)
input_file = args['<input_file>']
outfile_file = input_file + '.csv'
data = open(input_file, 'rb').read()

# Search for sync marker
SYNC_MARKER = bytes.fromhex('1A 91 4F F6 7F')
sync_offset = data.find(SYNC_MARKER)
if sync_offset == -1:
    raise ValueError('Sync marker not found in log file')
data = data[sync_offset + len(SYNC_MARKER):]
data = data.replace(SYNC_MARKER, b'')  # remove all other sync markers

header_row = [f'{value}' for topic in topics for value in topic]
rows = []

offset = 0
while offset < len(data):
    try:
        topic = struct.unpack_from('B', data, offset)[0]
        offset += 1

        if topic >= len(topics):
            raise ValueError(f'Invalid topic {topic} at offset {offset}')

        if topic == 0 or not rows:
            rows.append({})

        for name in topics[topic]:
            value = struct.unpack_from('<f', data, offset)[0]
            offset += 4
            rows[-1][name] = value
    except struct.error as e:
        break

# Write CSV file
with open(outfile_file, 'w', newline='') as f:
    writer = csv.DictWriter(f, fieldnames=header_row, extrasaction='ignore')
    writer.writeheader()
    rows = filter(lambda row: float(row.get('t', 0)), rows)
    writer.writerows(rows)
