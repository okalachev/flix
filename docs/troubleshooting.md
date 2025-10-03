# Troubleshooting

## The sketch doesn't compile

Do the following:

* **Check ESP32 core is installed**. Check if the version matches the one used in the [tutorial](usage.md#firmware).
* **Check libraries**. Install all the required libraries from the tutorial. Make sure there are no MPU9250 or other peripherals libraries that may conflict with the ones used in the tutorial.
* **Check the chosen board**. The correct board to choose in Arduino IDE for ESP32 Mini is *WEMOS D1 MINI ESP32*.

## The drone doesn't fly

Do the following:

* **Check the battery voltage**. Use a multimeter to measure the battery voltage. It should be in range of 3.7-4.2 V.
* **Check if there are some startup errors**. Connect the ESP32 to the computer and check the Serial Monitor output. Use the Reset button to make sure you see the whole ESP32 output.
* **Check the baudrate is correct**. If you see garbage characters in the Serial Monitor, make sure the baudrate is set to 115200.
* **Make sure correct IMU model is chosen**. If using ICM-20948 board, change `MPU9250` to `ICM20948` everywhere in the `imu.ino` file.
* **Check if the CLI is working**. Perform `help` command in Serial Monitor. You should see the list of available commands. You can also access the CLI using QGroundControl (*Vehicle Setup* ⇒ *Analyze Tools* ⇒ *MAVLink Console*).
* **Configure QGroundControl correctly before connecting to the drone** if you use it to control the drone. Go to the settings and enable *Virtual Joystick*. *Auto-Center Throttle* setting **should be disabled**.
* **If QGroundControl doesn't connect**, you might need to disable the firewall and/or VPN on your computer.
* **Check the IMU is working**. Perform `imu` command and check its output:
  * The `status` field should be `OK`.
  * The `rate` field should be about 1000 (Hz).
  * The `accel` and `gyro` fields should change as you move the drone.
* **Calibrate the accelerometer.** if is wasn't done before. Type `ca` command in Serial Monitor and follow the instructions.
* **Check the attitude estimation**. Connect to the drone using QGroundControl. Rotate the drone in different orientations and check if the attitude estimation shown in QGroundControl is correct.
* **Check the IMU orientation is set correctly**. If the attitude estimation is rotated, make sure `rotateIMU` function is defined correctly in `imu.ino` file.
* **Check the motors type**. Motors with exact 3.7V voltage are needed, not ranged working voltage (3.7V — 6V).
* **Check the motors**. Perform the following commands using Serial Monitor:
  * `mfr` — should rotate front right motor (counter-clockwise).
  * `mfl` — should rotate front left motor (clockwise).
  * `mrl` — should rotate rear left motor (counter-clockwise).
  * `mrr` — should rotate rear right motor (clockwise).
* **Calibrate the RC** if you use it. Type `cr` command in Serial Monitor and follow the instructions.
* **Check the RC data** if you use it. Use `rc` command, `Control` should show correct values between -1 and 1, and between 0 and 1 for the throttle.
* **Check the IMU output using QGroundControl**. Connect to the drone using QGroundControl on your computer. Go to the *Analyze* tab, *MAVLINK Inspector*. Plot the data from the `SCALED_IMU` message. The gyroscope and accelerometer data should change according to the drone movement.
