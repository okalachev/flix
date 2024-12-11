# Troubleshooting

## The sketch doesn't compile

Do the following:

* **Check ESP32 core is installed**. Check if the version matches the one used in the [tutorial](build.md#firmware).
* **Check libraries**. Install all the required libraries from the tutorial. Make sure there are no MPU9250 or other peripherals libraries that may conflict with the ones used in the tutorial.

## The drone doesn't fly

Do the following:

* **Check the battery voltage**. Use a multimeter to measure the battery voltage. It should be in range of 3.7-4.2 V.
* **Check if there are some startup errors**. Connect the ESP32 to the computer and check the Serial Monitor output. Use the Reset button to make sure you see the whole ESP32 output.
* **Make sure correct IMU model is chosen**. If using ICM-20948 board, change `MPU9250` to `ICM20948` everywhere in the `imu.ino` file.
* **Check if the CLI is working**. Perform `help` command in Serial Monitor. You should see the list of available commands.
* **Configure QGroundControl correctly before connecting to the drone** if you use it to control the drone. Go to the settings and enable *Virtual Joystick*. *Auto-Center Throttle* setting **should be disabled**.
* **Make sure you're not moving the drone several seconds after the power on**. The drone calibrates its gyroscope on the start so it should stay still for a while.
* **Check the IMU sample rate**. Perform `imu` command. The `rate` field should be about 1000 (Hz).
* **Check the IMU data**. Perform `imu` command, check raw accelerometer and gyro output. The output should change as you move the drone.
* **Calibrate the accelerometer.** if is wasn't done before. Perform `ca` command and put the results to `imu.ino` file.
* **Check the attitude estimation**. Connect to the drone using QGroundControl. Rotate the drone in different orientations and check if the attitude estimation shown in QGroundControl is correct.
* **Check the IMU orientation is set correctly**. If the attitude estimation is rotated, make sure `rotateIMU` function is defined correctly in `imu.ino` file.
* **Check the motors**. Perform the following commands using Serial Monitor:
  * `mfr` — should rotate front right motor (counter-clockwise).
  * `mfl` — should rotate front left motor (clockwise).
  * `mrl` — should rotate rear left motor (counter-clockwise).
  * `mrr` — should rotate rear right motor (clockwise).
* **Calibrate the RC** if you use it. Perform `rc` command and put the results to `rc.ino` file.
* **Check the RC data** if you use it. Use `rc` command, `Control` should show correct values between -1 and 1, and between 0 and 1 for the throttle.
* **Check the IMU output using QGroundControl**. Connect to the drone using QGroundControl on your computer. Go to the *Analyze* tab, *MAVLINK Inspector*. Plot the data from the `SCALED_IMU` message. The gyroscope and accelerometer data should change according to the drone movement.
* **Check the gyroscope only attitude estimation**. Comment out `applyAcc();` line in `estimate.ino` and check if the attitude estimation in QGroundControl. It should be stable, but only drift very slowly.
