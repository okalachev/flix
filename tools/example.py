#!/usr/bin/env python3

import math
from pyflix import Flix

print('=== Connect...')
flix = Flix()

print('Connected:', flix.connected)
print('Mode:', flix.mode)
print('Armed:', flix.armed)
print('Landed:', flix.landed)
print('Rates:', *[f'{math.degrees(r):.0f}°/s' for r in flix.rates])
print('Attitude:', *[f'{math.degrees(a):.0f}°' for a in flix.attitude_euler])
print('Motors:', flix.motors)
print('Acc', flix.acc)
print('Gyro', flix.gyro)

print('=== Execute commands...')
print('> time')
print(flix.cli('time'))
print('> imu')
print(flix.cli('imu'))

print('=== Get parameter...')
pitch_p = flix.get_param('PITCH_P')
print('PITCH_P = ', pitch_p)

print('=== Set parameter...')
flix.set_param('PITCH_P', pitch_p)

print('=== Wait for gyro update...')
print('Gyro: ', flix.wait('gyro'))

print('=== Wait for HEARTBEAT message...')
print(flix.wait('mavlink.HEARTBEAT'))

print('=== When until landed = False (remove drone from the surface)')
flix.wait('landed', value=False)
