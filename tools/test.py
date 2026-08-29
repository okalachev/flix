#!/usr/bin/env python3

# Script for testing pyflix and the simulation.

from pytest import approx, raises
from math import isnan, isfinite
import time
from pyflix import Flix, mavlink

def test():
    print('=== Connect...')
    flix = Flix(timeout=20)

    print('=== Check initial state')
    time.sleep(1)  # give more time for initial state
    assert flix.connected
    assert flix.mode == 'STAB'
    assert not flix.armed
    assert flix.landed
    assert isnan(flix.voltage) or flix.voltage == approx(4.2)
    assert flix.rates == approx((0, 0, 0), abs=0.01)
    assert flix.attitude == approx((1, 0, 0, 0), abs=0.01)
    assert flix.attitude_euler == approx((0, 0, 0), abs=0.01)
    assert all(m == 0 for m in flix.motors)
    assert flix.acc == approx((0, 0, 9.81), abs=0.1)
    assert flix.gyro == approx((0, 0, 0), abs=0.01)
    assert all(ch == 0 for ch in flix.channels)

    print('=== Check console commands')
    assert 'Time: ' in flix.cli('time')
    assert 'landed: 1' in flix.cli('imu')

    print('=== Check parameters')
    assert isfinite(flix.get_param('CTL_ATT_P_P'))
    flix.set_param('CTL_ATT_P_P', 10.0)

    print('=== Additional checks')
    assert flix.wait('gyro') == approx((0, 0, 0), abs=0.01)
    flix.wait('armed', False)
    flix.wait('mode', 'STAB')
    flix.wait('motors', lambda motors: not any(motors))
    flix.set_armed(True)
    flix.wait('armed', True)
    flix.set_mode('ACRO')
    flix.wait('mode', 'ACRO')
    flix.set_mode('AUTO')
    flix.wait('mode', 'AUTO')

    print("=== Check command errors")
    with raises(RuntimeError, match='MAV_RESULT_DENIED'):
        flix._command_send(mavlink.MAV_CMD_DO_SET_MODE, [0, 99, 0, 0, 0, 0, 0])  # invalid mode
    with raises(RuntimeError, match='MAV_RESULT_UNSUPPORTED'):
        flix._command_send(mavlink.MAV_CMD_DO_PARACHUTE, [0, 0, 0, 0, 0, 0, 0])  # unsupported command
