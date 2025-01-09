# Flix version 0

Flix version 0 (obsolete):

<img src="img/flix.jpg" width=500 alt="Flix quadcopter">

## Components list

|Type|Part|Image|Quantity|
|-|-|-|-|
|Microcontroller board|ESP32 Mini|<img src="img/esp32.jpg" width=100>|1|
|IMU and barometer² board|GY-91 (or other MPU-9250 board)|<img src="img/gy-91.jpg" width=100>|1|
|Quadcopter frame|K100|<img src="img/frame.jpg" width=100>|1|
|Motor|8520 3.7V brushed motor (**shaft 0.8mm!**)|<img src="img/motor.jpeg" width=100>|4|
|Propeller|Hubsan 55 mm|<img src="img/prop.jpg" width=100>|4|
|Motor ESC|2.7A 1S Dual Way Micro Brush ESC|<img src="img/esc.jpg" width=100>|4|
|RC transmitter|KINGKONG TINY X8|<img src="img/tx.jpg" width=100>|1|
|RC receiver|DF500 (SBUS)|<img src="img/rx.jpg" width=100>|1|
|~~SBUS inverter~~*||<img src="img/inv.jpg" width=100>|~~1~~|
|Battery|3.7 Li-Po 850 MaH 60C|||
|Battery charger||<img src="img/charger.jpg" width=100>|1|
|Wires, connectors, tape, ...||||

*\* — not needed as ESP32 supports [software pin inversion](https://github.com/bolderflight/sbus#inverted-serial).*

## Schematics

<img src="img/schematics.svg" width=800 alt="Flix schematics">

You can also check a user contributed [variant of complete circuit diagram](https://miro.com/app/board/uXjVN-dTjoo=/?moveToWidget=3458764574482511443&cot=14) of the drone.
