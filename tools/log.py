#!/usr/bin/env python3

# Download flight log remotely and save to file

import os
import datetime
from pyflix import Flix

DIR = os.path.dirname(os.path.realpath(__file__))

flix = Flix()

print('Downloading log...')
lines = flix.cli('log').splitlines()

# sort by timestamp
header = lines.pop(0)
lines.sort(key=lambda line: float(line.split(',')[0]))

log = open(f'{DIR}/log/{datetime.datetime.now().isoformat()}.csv', 'wb')
content = header.encode() + b'\n' + b'\n'.join(line.encode() for line in lines)
log.write(content)
print(f'Written {os.path.relpath(log.name, os.curdir)}')
