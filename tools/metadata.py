#!/usr/bin/env python3

# Script for checking the metadata files

import sys, os
import re
from pathlib import Path
import shutil
import json
import zlib
import requests

def mavlink_crc323(data: bytes) -> int:
    return (zlib.crc32(data, 0xFFFFFFFF) ^ 0xFFFFFFFF) & 0xFFFFFFFF


code = 0  # exit code
parameter_regexp = re.compile(r'^\s+\{"(?P<name>[^"]+)",\s*&(?P<variable>[^,]+)(.*)\},?$', re.MULTILINE)
parameters_source_file = Path(__file__).parent.parent / 'flix' / 'parameters.ino'
mavlink_source_file = Path(__file__).parent.parent / 'flix' / 'mavlink.ino'
parameters_code = parameters_source_file.read_text()
mavlink_code = mavlink_source_file.read_text()
parameters_meta_file = Path(__file__).parent / 'parameters.json'
parameters_meta = json.loads(parameters_meta_file.read_text())
assert parameters_meta['version'] == 1

parameters = parameter_regexp.finditer(parameters_code)

for parameter in parameters:
    name = parameter.group('name')
    variable = parameter.group('variable')

    for meta in parameters_meta['parameters']:
        name_meta = meta['name']
        regex = re.sub(r'\{[^\}]+\}', r'.*', name_meta) # handle {n} wildcards
        if re.fullmatch(regex, name):
            break
    else:
        raise RuntimeError(f'Parameter {name} is missing in parameters.json')

general_meta_file = Path(__file__).parent / 'general.json'
general_crc = mavlink_crc323(general_meta_file.read_bytes())
general_data = json.loads(general_meta_file.read_text())

parameters_crc = mavlink_crc323(parameters_meta_file.read_bytes())

general_url = f'https://quadcopter.dev/meta/{general_crc}/general.json'
parameters_url = f'https://quadcopter.dev/meta/{parameters_crc}/parameters.json'

print(f'General CRC: {general_crc}')
print(f'Parameters CRC: {mavlink_crc323(parameters_meta_file.read_bytes())}')
print(f'General URL: {general_url}')
print(f'Parameters URL: {parameters_url}')

assert general_data['version'] == 1
for meta in general_data['metadataTypes']:
    if meta['type'] == 1:
        if meta['uri'] != parameters_url:
            print(f'ERROR: Parameters URI should be {parameters_url}')
            code = 1
        if meta['fileCrc'] != parameters_crc:
            print(f'ERROR: Parameters CRC shoule be {parameters_crc}')
            code = 1
    break
else:
    print('ERROR: Parameters metadata not found in general.json')
    code = 1

# check mavlink.ino contains correct link
if f'{general_crc},' not in mavlink_code:
    print(f'ERROR: mavlink.ino does not contain correct general.json CRC: {general_crc}')
    code = 1
if general_url not in mavlink_code:
    print(f'ERROR: mavlink.ino does not contain correct general.json URL: {general_url}')
    code = 1

if sys.argv[-1] == 'generate' and not code:
    print('Copy metadata files')
    os.makedirs(Path(__file__).parent.parent / 'meta' / str(general_crc), exist_ok=True)
    os.makedirs(Path(__file__).parent.parent / 'meta' / str(parameters_crc), exist_ok=True)
    shutil.copy(general_meta_file, Path(__file__).parent.parent / 'meta' / str(general_crc) / 'general.json')
    shutil.copy(parameters_meta_file, Path(__file__).parent.parent / 'meta' / str(parameters_crc) / 'parameters.json')

if code == 0:
    print('Everything is good')
sys.exit(code)

