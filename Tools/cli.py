#!/usr/bin/env python3

# Remote CLI for Flix

from pyflix import Flix

flix = Flix()

flix.on('print', lambda text: print(text, end=''))

while True:
    command = input()
    flix.cli(command, wait_response=False)
