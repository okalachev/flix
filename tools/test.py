#!/usr/bin/env python3

# Script for testing pyflix and the simulation.

from math import isclose, isnan, isfinite
from pyflix import Flix

print('=== Connect...')
flix = Flix()

print('=== Check initial state')
assert flix.connected
assert flix.mode == 'STAB'
assert not flix.armed
assert flix.landed
assert isnan(flix.voltage)
assert all(isclose(r, 0) for r in flix.rates)
assert isclose(flix.attitude[0], 0) and isclose(flix.attitude[1], 0) and isclose(flix.attitude[2], 0)
assert isclose(flix.attitude[3], 1)
assert all(isclose(a, 0) for a in flix.attitude_euler)
assert all(m == 0 for m in flix.motors)
assert all(isclose(a, 0) for a in flix.acc)
assert all(isclose(g, 0) for g in flix.gyro)
assert all(ch == 0 for ch in flix.channels)

print('=== Check console commands')
assert 'Time: ' in flix.cli('time')
assert 'Landed: 1' in flix.cli('imu')

print('=== Check parameters')
assert isfinite(flix.get_param('CTL_ATT_P_P'))
flix.set_param('CTL_ATT_P_P', 10.0)

print('=== Additional checks')
assert flix.wait('connected') is True
assert flix.wait('gyro') is not None
flix.wait('armed', False)
flix.wait('mode', 'STAB')
flix.wait('motors', lambda motors: not any(motors))
flix.set_armed(True)
flix.wait('armed', True)
flix.set_mode('ACRO')
flix.wait('mode', 'ACRO')
flix.set_mode('AUTO')
flix.wait('mode', 'AUTO')
