#!/usr/bin/env python3

# Test RGB animation

import time
from pyflix import Flix

flix = Flix()
led_count = int(flix.get_param('LED_COUNT'))

colors = [
    (255, 0, 0),
    (0, 255, 0),
    (0, 0, 255),
    (255, 255, 0),
    (255, 0, 255),
    (0, 255, 255),
    (255, 255, 255),
]

try:
    while True:
        frame = []
        for i in range(led_count):
            r, g, b = colors[(i + int(time.time() * 3)) % len(colors)]
            frame.append([i, r, g, b])

        flix.set_led(frame)
        time.sleep(0.1)
except KeyboardInterrupt:
    flix.set_led([[i, 0, 0, 0] for i in range(led_count)])
