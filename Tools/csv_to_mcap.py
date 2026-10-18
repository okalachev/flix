#!/usr/bin/env python3

"""Convert CSV log file to MCAP file.

Usage:
  csv_to_mcap.py <csv_file> [<mcap_file>]
"""

import csv
import json
import docopt
from mcap.writer import Writer

args = docopt.docopt(__doc__)
input_file = args['<csv_file>']
output_file = args['<mcap_file>'] or input_file.replace('.csv', '.mcap')
if input_file == output_file:
    raise ValueError('Input and output files are the same')

csv_file = open(input_file, 'r')
csv_reader = csv.reader(csv_file, delimiter=',')
header = next(csv_reader)

mcap_file = open(output_file, 'wb')
writer = Writer(mcap_file)
writer.start()

properties = {key: {'type': 'number'} for key in header}
schema_id = writer.register_schema(
    name="state",
    encoding="jsonschema",
    data=json.dumps({"type": "object", "properties": properties}).encode(),
)

channel_id = writer.register_channel(
    schema_id=schema_id,
    topic="state",
    message_encoding="json",
)

for row in csv_reader:
    data = {key: float(value) for key, value in zip(header, row)}
    timestamp = round(float(row[0]) * 1e9)
    writer.add_message(channel_id=channel_id, log_time=timestamp, data=json.dumps(data).encode(), publish_time=timestamp,)

writer.finish()
