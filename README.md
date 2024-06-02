# flix

**flix** (*flight + X*) — making an open source ESP32-based quadcopter from scratch.

<img src="docs/img/flix.jpg" width=500 alt="Flix quadcopter">

## Features

* Simple and clean Arduino based source code.
* Acro and Stabilized flight using remote control.
* Precise simulation using Gazebo.
* [In-RAM logging](docs/log.md).
* Command line interface through USB port.
* Wi-Fi support.
* MAVLink support.
* Control using mobile phone (with QGroundControl app).
* ESCs with reverse mode support.
* *Textbook and videos for students on writing a flight controller\*.*
* *Completely 3D-printed frame*.*
* *Position control and autonomous flights using external camera\**.
* [Building and running instructions](docs/build.md).

*\* — planned.*

## It actually flies

<a href="https://youtu.be/8GzzIQ3C6DQ"><img width=500 src="https://i3.ytimg.com/vi/8GzzIQ3C6DQ/maxresdefault.jpg"></a>

See YouTube demo video: https://youtu.be/8GzzIQ3C6DQ.

## Simulation

Simulation in Gazebo using a plugin that runs original Arduino code is implemented:

<img src="docs/img/simulator.png" width=500 alt="Flix simulator">

## Schematics

<img src="docs/img/schematics.svg" width=800 alt="Flix schematics">

You can also check a user contributed [variant of complete circuit diagram](https://miro.com/app/board/uXjVN-dTjoo=/) of the drone.

*\* — SBUS inverter is not needed as ESP32 supports [software pin inversion](https://github.com/bolderflight/sbus#inverted-serial).*

## Components (version 0)

|Component|Type|Image|Quantity|
|-|-|-|-|
|ESP32 Mini|Microcontroller board|<img src="docs/img/esp32.jpg" width=100>|1|
|GY-91|IMU+LDO+barometer board|<img src="docs/img/gy-91.jpg" width=100>|1|
|K100|Quadcopter frame|<img src="docs/img/frame.jpg" width=100>|1|
|8520 3.7V brushed motor (**shaft 0.8mm!**)|Motor|<img src="docs/img/motor.jpeg" width=100>|4|
|Hubsan 55 mm| Propeller|<img src="docs/img/prop.jpg" width=100>|4|
|2.7A 1S Dual Way Micro Brush ESC|Motor ESC|<img src="docs/img/esc.jpg" width=100>|4|
|KINGKONG TINY X8|RC transmitter|<img src="docs/img/tx.jpg" width=100>|1|
|DF500 (SBUS)|RC receiver|<img src="docs/img/rx.jpg" width=100>|1|
||~~SBUS inverter~~*|<img src="docs/img/inv.jpg" width=100>|~~1~~|
|3.7 Li-Po 850 MaH 60C|Battery|||
||Battery charger|<img src="docs/img/charger.jpg" width=100>|1|
||Wires, connectors, tape, ...|||
||3D-printed frame parts|||

*\* — not needed as ESP32 supports [software pin inversion](https://github.com/bolderflight/sbus#inverted-serial).*

## Materials

Subscribe to Telegram-channel on developing the drone and the flight controller (in Russian): https://t.me/opensourcequadcopter.

Detailed article on Habr.com about the development of the drone (in Russian): https://habr.com/ru/articles/814127/.
