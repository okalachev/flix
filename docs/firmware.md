# Firmware overview

## Dataflow

<img src="img/dataflow.svg" width=800 alt="Firmware dataflow diagram">

The main loop is running at 1000 Hz. All the dataflow is happening through global variables (for simplicity):

* `t` *(float)* — current step time, *s*.
* `dt` *(float)* — time delta between the current and previous steps, *s*.
* `gyro` *(Vector)* — data from the gyroscope, *rad/s*.
* `acc` *(Vector)* — acceleration data from the accelerometer, *m/s<sup>2</sup>*.
* `rates` *(Vector)* — filtered angular rates, *rad/s*.
* `attitude` *(Quaternion)* — estimated attitude (orientation) of drone.
* `controls` *(float[])* — user control inputs from the RC, normalized to [-1, 1] range.
* `motors` *(float[])* — motor outputs, normalized to [-1, 1] range; reverse rotation is possible.

## Source files

Firmware source files are located in `flix` directory. The key files are:

* [`flix.ino`](../flix/flix.ino) — main entry point, Arduino sketch. Includes global variables definition and the main loop.
* [`imu.ino`](../flix/imu.ino) — reading data from the IMU sensor (gyroscope and accelerometer), IMU calibration.
* [`rc.ino`](../flix/rc.ino) — reading data from the RC receiver, RC calibration.
* [`estimate.ino`](../flix/estimate.ino) — drone's attitude estimation, complementary filter.
* [`control.ino`](../flix/control.ino) — drone's attitude and rates control, three-dimensional two-level cascade PID controller.
* [`motors.ino`](../flix/motors.ino) — PWM motor outputs control.

Utility files include:

* [`vector.h`](../flix/vector.h), [`quaternion.h`](../flix/quaternion.h) — project's vector and quaternion libraries implementation.
* [`pid.h`](../flix/pid.h) — generic PID controller implementation.
* [`lpf.h`](../flix/lpf.h) — generic low-pass filter implementation.

## Building

See build instructions in [build.md](build.md).
