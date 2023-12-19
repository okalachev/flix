# Building and running

## Simulation

Dependencies are [Gazebo Classic simulator](https://classic.gazebosim.org) and [SDL2](https://www.libsdl.org) library.

### Ubuntu

1. Install Gazebo 11:

   ```bash
   curl -sSL http://get.gazebosim.org | sh
   ```

   Set up your Gazebo environment variables:

   ```bash
   echo "source /usr/share/gazebo/setup.sh" >> ~/.bashrc
   source ~/.bashrc
   ```

2. Install SDL2:

   ```bash
   sudo apt-get install libsdl2-dev
   ```

3. Run the simulation:

   ```bash
   make simulator
   ```

### macOS

1. Install Homebrew package manager, if you don't have it installed:

   ```bash
   /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
   ```

2. Install Gazebo 11 and SDL2:

   ```bash
   brew tap osrf/simulation
   brew install gazebo11
   brew install sdl2
   ```

3. Run the simulation:

   ```bash
   make simulator
   ```

## Firmware

### Arduino IDE (Windows, Linux, macOS)

1. Install [Arduino IDE](https://www.arduino.cc/en/software).
2. Install ESP32 core using [Boards Manager](https://docs.arduino.cc/learn/starting-guide/cores).
3. Build and upload the firmware using Arduino IDE.

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
