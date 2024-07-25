# Building and running

To build the firmware or the simulator, you need to clone the repository using git:

```bash
git clone https://github.com/okalachev/flix.git
cd flix
```

## Simulation

### Ubuntu

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

### Flight

Use USB remote control or [QGroundControl mobile app](https://docs.qgroundcontrol.com/master/en/qgc-user-guide/getting_started/download_and_install.html#android) (with *Virtual Joystick* setting enabled) to control the drone. *Auto-Center Throttle* setting **should be disabled**.

## Firmware

### Arduino IDE (Windows, Linux, macOS)

1. Install [Arduino IDE](https://www.arduino.cc/en/software) (version 2 is recommended).
2. Install ESP32 core using [Boards Manager](https://docs.arduino.cc/learn/starting-guide/cores).
3. Install the following libraries using [Library Manager](https://docs.arduino.cc/software/ide-v2/tutorials/ide-v2-installing-a-library):
   * `FlixPeriph`.
   * `MAVLink`, version 2.0.1.
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

### Firmware code structure

See [firmware overview](firmware.md) for more details.

## Setup

Before flight in simulation and on the real drone, you need to calibrate your remote control. Use drone's command line interface (`make monitor` on the real drone) and type `cr` command. Copy calibration results to the source code (`flix/rc.ino` and/or `gazebo/joystick.h`).

On the real drone, you also need to calibrate the accelerometer and the gyroscope. Use `ca` and `cg` commands for that. Copy calibration results to the source code (`flix/imu.ino`).
