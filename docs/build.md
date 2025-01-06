# Building and running

To build the firmware or the simulator, you need to clone the repository using git:

```bash
git clone https://github.com/okalachev/flix.git
cd flix
```

## Simulation

### Ubuntu 20.04

The latest version of Ubuntu supported by Gazebo 11 simulator is 20.04. If you have a newer version, consider using a virtual machine.

1. Install Arduino CLI:

   ```bash
   curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR=~/.local/bin sh
   ```

2. Install Gazebo 11:

   ```bash
   curl -sSL http://get.gazebosim.org | sh
   ```

   Set up your Gazebo environment variables:

   ```bash
   echo "source /usr/share/gazebo/setup.sh" >> ~/.bashrc
   source ~/.bashrc
   ```

3. Install SDL2 and other dependencies:

   ```bash
   sudo apt-get update && sudo apt-get install build-essential libsdl2-dev
   ```

4. Add your user to the `input` group to enable joystick support (you need to re-login after this command):

   ```bash
   sudo usermod -a -G input $USER
   ```

5. Run the simulation:

   ```bash
   make simulator
   ```

### macOS

1. Install Homebrew package manager, if you don't have it installed:

   ```bash
   /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
   ```

2. Install Arduino CLI, Gazebo 11 and SDL2:

   ```bash
   brew tap osrf/simulation
   brew install arduino-cli
   brew install gazebo11
   brew install sdl2
   ```

   Set up your Gazebo environment variables:

   ```bash
   echo "source /opt/homebrew/share/gazebo/setup.sh" >> ~/.zshrc
   source ~/.zshrc
   ```

3. Run the simulation:

   ```bash
   make simulator
   ```

### Setup and flight

#### Control with smartphone

1. Install [QGroundControl mobile app](https://docs.qgroundcontrol.com/master/en/qgc-user-guide/getting_started/download_and_install.html#android) on your smartphone.
2. Connect your smartphone to the same Wi-Fi network as the machine running the simulator.
3. If you're using a virtual machine, make sure that its network is set to the **bridged** mode with Wi-Fi adapter selected.
4. Run the simulation.
5. Open QGroundControl app. It should connect and begin showing the virtual drone's telemetry automatically.
6. Go to the settings and enable *Virtual Joystick*. *Auto-Center Throttle* setting **should be disabled**.
7. Use the virtual joystick to fly the drone!

#### Control with USB remote control

1. Connect your USB remote control to the machine running the simulator.
2. Run the simulation.
3. Calibrate the RC using `cr` command in the command line interface and stop the simulation.
4. Copy the calibration results to the source code (`gazebo/joystick.h`).
5. Run the simulation again.
6. Use the USB remote control to fly the drone!

## Firmware

### Arduino IDE (Windows, Linux, macOS)

1. Install [Arduino IDE](https://www.arduino.cc/en/software) (version 2 is recommended).
2. Install ESP32 core, version 3.1.0 (version 2.x is not supported). See the [official Espressif's instructions](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html#installing-using-arduino-ide) on installing ESP32 Core in Arduino IDE.
3. Install the following libraries using [Library Manager](https://docs.arduino.cc/software/ide-v2/tutorials/ide-v2-installing-a-library):
   * `FlixPeriph`, the latest version.
   * `MAVLink`, version 2.0.12.
4. Clone the project using git or [download the source code as a ZIP archive](https://codeload.github.com/okalachev/flix/zip/refs/heads/master).
5. Open the downloaded Arduino sketch `flix/flix.ino` in Arduino IDE.
6. [Build and upload](https://docs.arduino.cc/software/ide-v2/tutorials/getting-started/ide-v2-uploading-a-sketch) the firmware using Arduino IDE.

### Command line (Windows, Linux, macOS)

1. [Install Arduino CLI](https://arduino.github.io/arduino-cli/installation/).
2. Windows users might need to install [USB to UART bridge driver from Silicon Labs](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers).
3. Compile the firmware using `make`. Arduino dependencies will be installed automatically:

   ```bash
   make
   ```

   You can flash the firmware to the board using command:

   ```bash
   make upload
   ```

   You can also compile the firmware, upload it and start serial port monitoring using command:

   ```bash
   make upload monitor
   ```

See other available Make commands in the [Makefile](../Makefile).

### Setup and flight

Before flight you need to calibrate the accelerometer:

1. Open Serial Monitor in Arduino IDE (use use `make monitor` command in the command line).
2. Type `ca` command there.
3. Copy calibration results to the source code (`flix/imu.ino`).

#### Control with smartphone

1. Install [QGroundControl mobile app](https://docs.qgroundcontrol.com/master/en/qgc-user-guide/getting_started/download_and_install.html#android) on your smartphone.
2. Power the drone using the battery.
3. Connect your smartphone to the appeared `flix` Wi-Fi network.
4. Open QGroundControl app. It should connect and begin showing the drone's telemetry automatically.
5. Go to the settings and enable *Virtual Joystick*. *Auto-Center Throttle* setting **should be disabled**.
6. Use the virtual joystick to fly the drone!

#### Control with remote control

Before flight using remote control, you need to calibrate it:

1. Open Serial Monitor in Arduino IDE (use use `make monitor` command in the command line).
2. Type `cr` command there.
3. Copy calibration results to the source code (`flix/rc.ino`).

Then you can use your remote control to fly the drone!

> [!NOTE]
> If something goes wrong, go to the [Troubleshooting](troubleshooting.md) article.

### Firmware code structure

See [firmware overview](firmware.md) for more details.
