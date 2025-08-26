# Архитектура прошивки

Прошивка Flix это обычный скетч Arduino, реализованный в однопоточном стиле. Код инициализации находится в функции `setup()`, а главный цикл — в функции `loop()`. Скетч состоит из нескольких файлов, каждый из которых отвечает за определенную подсистему.

<img src="img/dataflow.svg" width=600 alt="Firmware dataflow diagram">

Главный цикл `loop()` работает на частоте 1000 Гц. Передача данных между подсистемами происходит через глобальные переменные:

* `t` *(float)* — текущее время шага, *с*.
* `dt` *(float)* — дельта времени между текущим и предыдущим шагами, *с*.
* `gyro` *(Vector)* — данные с гироскопа, *рад/с*.
* `acc` *(Vector)* — данные с акселерометра, *м/с<sup>2</sup>*.
* `rates` *(Vector)* — отфильтрованные угловые скорости, *рад/с*.
* `attitude` *(Quaternion)* — оценка ориентации (положения) дрона.
* `controlRoll`, `controlPitch`, ... *(float[])* — команды управления от пилота, в диапазоне [-1, 1].
* `motors` *(float[])* — выходные сигналы на моторы, в диапазоне [0, 1].

## Исходные файлы

Исходные файлы прошивки находятся в директории `flix`. Основные файлы:

* [`flix.ino`](https://github.com/okalachev/flix/blob/master/flix/flix.ino) — основной файл Arduino-скетча. Определяет некоторые глобальные переменные и главный цикл.
* [`imu.ino`](https://github.com/okalachev/flix/blob/master/flix/imu.ino) — чтение данных с датчика IMU (гироскоп и акселерометр), калибровка IMU.
* [`rc.ino`](https://github.com/okalachev/flix/blob/master/flix/rc.ino) — чтение данных с RC-приемника, калибровка RC.
* [`estimate.ino`](https://github.com/okalachev/flix/blob/master/flix/estimate.ino) — оценка ориентации дрона, комплементарный фильтр.
* [`control.ino`](https://github.com/okalachev/flix/blob/master/flix/control.ino) — подсистема управления, трехмерный двухуровневый каскадный ПИД-регулятор.
* [`motors.ino`](https://github.com/okalachev/flix/blob/master/flix/motors.ino) — выход PWM на моторы.
* [`mavlink.ino`](https://github.com/okalachev/flix/blob/master/flix/mavlink.ino) — взаимодействие с QGroundControl или [pyflix](https://github.com/okalachev/flix/tree/master/tools/pyflix) через протокол MAVLink.

Вспомогательные файлы:

* [`vector.h`](https://github.com/okalachev/flix/blob/master/flix/vector.h), [`quaternion.h`](https://github.com/okalachev/flix/blob/master/flix/quaternion.h) — библиотеки векторов и кватернионов.
* [`pid.h`](https://github.com/okalachev/flix/blob/master/flix/pid.h) — ПИД-регулятор.
* [`lpf.h`](https://github.com/okalachev/flix/blob/master/flix/lpf.h) — фильтр нижних частот.

### Подсистема управления

Состояние органов управления обрабатывается в функции `interpretControls()` и преобразуется в *команду управления*, которая включает следующее:

* `attitudeTarget` *(Quaternion)* — целевая ориентация дрона.
* `ratesTarget` *(Vector)* — целевые угловые скорости, *рад/с*.
* `ratesExtra` *(Vector)* — дополнительные (feed-forward) угловые скорости, для управления рысканием в режиме STAB, *рад/с*.
* `torqueTarget` *(Vector)* — целевой крутящий момент, диапазон [-1, 1].
* `thrustTarget` *(float)* — целевая общая тяга, диапазон [0, 1].

Команда управления обрабатывается в функциях `controlAttitude()`, `controlRates()`, `controlTorque()`. Если значение одной из переменных установлено в `NAN`, то соответствующая функция пропускается.

<img src="img/control.svg" width=300 alt="Control subsystem diagram">

Состояние *armed* хранится в переменной `armed`, а текущий режим — в переменной `mode`.
