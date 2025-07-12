# Архитектура прошивки

<img src="img/dataflow.svg" width=800 alt="Firmware dataflow diagram">

Главный цикл работает на частоте 1000 Гц. Передача данных между подсистемами происходит через глобальные переменные:

* `t` *(float)* — текущее время шага, *с*.
* `dt` *(float)* — дельта времени между текущим и предыдущим шагами, *с*.
* `gyro` *(Vector)* — данные с гироскопа, *рад/с*.
* `acc` *(Vector)* — данные с акселерометра, *м/с<sup>2</sup>*.
* `rates` *(Vector)* — отфильтрованные угловые скорости, *рад/с*.
* `attitude` *(Quaternion)* — оценка ориентации (положения) дрона.
* `controls` *(float[])* — команды управления от пилота, в диапазоне [-1, 1].
* `motors` *(float[])* — выходные сигналы на моторы, в диапазоне [0, 1].

## Исходные файлы

Исходные файлы прошивки находятся в директории `flix`. Ключевые файлы:

* [`flix.ino`](https://github.com/okalachev/flix/blob/canonical/flix/flix.ino) — основной входной файл, скетч Arduino. Включает определение глобальных переменных и главный цикл.
* [`imu.ino`](https://github.com/okalachev/flix/blob/canonical/flix/imu.ino) — чтение данных с датчика IMU (гироскоп и акселерометр), калибровка IMU.
* [`rc.ino`](https://github.com/okalachev/flix/blob/canonical/flix/rc.ino) — чтение данных с RC-приемника, калибровка RC.
* [`mavlink.ino`](https://github.com/okalachev/flix/blob/canonical/flix/mavlink.ino) — взаимодействие с QGroundControl через MAVLink.
* [`estimate.ino`](https://github.com/okalachev/flix/blob/canonical/flix/estimate.ino) — оценка ориентации дрона, комплементарный фильтр.
* [`control.ino`](https://github.com/okalachev/flix/blob/canonical/flix/control.ino) — управление ориентацией и угловыми скоростями дрона, трехмерный двухуровневый каскадный PID-регулятор.
* [`motors.ino`](https://github.com/okalachev/flix/blob/canonical/flix/motors.ino) — управление выходными сигналами на моторы через ШИМ.

Вспомогательные файлы включают:

* [`vector.h`](https://github.com/okalachev/flix/blob/canonical/flix/vector.h), [`quaternion.h`](https://github.com/okalachev/flix/blob/canonical/flix/quaternion.h) — реализация библиотек векторов и кватернионов проекта.
* [`pid.h`](https://github.com/okalachev/flix/blob/canonical/flix/pid.h) — реализация общего ПИД-регулятора.
* [`lpf.h`](https://github.com/okalachev/flix/blob/canonical/flix/lpf.h) — реализация общего фильтра нижних частот.
