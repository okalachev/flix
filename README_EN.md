<div align="center">

# 🚁 Flix Flight Controller · YaoKun Chinese Edition

**An open-source ESP32 quadcopter firmware written from scratch — under 2000 lines of Arduino code, built for education and research**

![MCU](https://img.shields.io/badge/MCU-ESP32%20%7C%20ESP32--S3%20%7C%20ESP32--C3-purple)

![Framework](https://img.shields.io/badge/Framework-Arduino-blue)

![Protocol](https://img.shields.io/badge/Protocol-MAVLink%20%7C%20ESP--NOW-green)

![Language](https://img.shields.io/badge/Language-C%2B%2B17-orange)

![Upstream](https://img.shields.io/badge/Upstream-okalachev%2Fflix-lightgrey)

[**简体中文**](README.md) · **English**

</div>

---

> 📌 **This repository is a Chinese-documented fork of [okalachev/flix](https://github.com/okalachev/flix), plus a YaoKun S3 hardware port.**  
> The `master` branch is kept **identical to upstream** (only the directory layout changed,  
> not a single line of logic). The `yaokun-organized` branch adds **hardware adaptation and a  
> Chinese UI** on top of that — see [YaoKun S3 Edition](#-yaokun-s3-edition-yaokun-organized-branch).  
> Upstream author: [Oleg Kalachev](https://github.com/okalachev) · Project site: [quadcopter.dev](https://quadcopter.dev)

## 📌 What is it

Flix is an **open-source quadcopter firmware designed for education and research**, running on ESP32.

Its defining trait is **simplicity**: the entire firmware is written in Arduino C/C++ in  
**under 2000 lines** — no RTOS, no closed-source libraries, no black-box algorithms.  
Sensor drivers, attitude estimation, PID control, motor mixing and MAVLink communication  
are all laid out in front of you. Read it and you genuinely understand how a flight controller works.

![Flix Gazebo simulator](Docs/img/simulator1.png)

## ✨ Features

| Feature                      | Description                                                                                          |
| ---------------------------- | ---------------------------------------------------------------------------------------------------- |
| **Tiny codebase**            | Arduino, firmware **< 2000 lines**, every line readable and hackable                                 |
| **Common parts only**        | Built from widely available components — no custom fabrication needed                                |
| **MAVLink protocol**         | Over Wi-Fi or ESP-NOW, works with QGroundControl and friends                                         |
| **Multiple control options** | USB gamepad / RC transmitter / smartphone, via Wi-Fi or ESP-NOW                                      |
| **Wireless CLI**             | No cable needed — connect over Wi-Fi and run commands from phone or PC                               |
| **Gazebo simulation**        | The simulator runs the **very same Arduino source** — no flashing required                           |
| **Python library**           | `pyflix` lets you script and automate flights                                                        |
| **Textbook included**        | A systematic course on flight-control theory and practice ([in development](https://quadcopter.dev)) |
| **Multi-chip**               | One codebase for ESP32 / ESP32-S3 / ESP32-C3 — switch boards with `BOARD`                            |

## 🚁 It actually flies

Demo video: <https://youtu.be/hT46CZ1CgC4>

<a href="https://youtu.be/hT46CZ1CgC4"><img width=500 src="https://i3.ytimg.com/vi/hT46CZ1CgC4/maxresdefault.jpg"></a>

Version 0 demo video: <https://youtu.be/8GzzIQ3C6DQ>

<a href="https://youtu.be/8GzzIQ3C6DQ"><img width=500 src="https://i3.ytimg.com/vi/8GzzIQ3C6DQ/maxresdefault.jpg"></a>

Usage in education (RoboCamp 2025): <https://youtu.be/Wd3yaorjTx0>

<a href="https://youtu.be/Wd3yaorjTx0"><img width=500 src="https://i3.ytimg.com/vi/Wd3yaorjTx0/sddefault.jpg"></a>

See the [user builds gallery](Docs/user.md):

<a href="Docs/user.md"><img src="Docs/img/user/user.jpg" width=500></a>

## 📁 Directory layout (reorganized in this fork)

> 🔧 **Why reorganize**: upstream keeps firmware, docs and Python tools flat in the repo root,  
> so `.ino` sources, scripts, an mdBook textbook and Gazebo models all sit side by side.  
> This fork regroups everything **by purpose**, with **everything firmware-related under `Firmware/`**.  
> Crucially, `Firmware/` is **a self-contained, independently buildable project** —  
> so `make` in the repo root still compiles out of the box, with no argument changes.

```text
flix/                          # ← repo root: docs + forwarding Makefile
├── README.md                  # Chinese readme (default)
├── README_EN.md               # This file
├── Makefile                   # Forwarder: root `make` → Firmware/Makefile
├── Firmware/                  # ★ Firmware project (buildable on its own)
│   ├── Makefile               #   Real build script (compile / flash / simulate)
│   ├── flix/                  #   Arduino sketch — all flight code
│   │   ├── flix.ino           #     Entry point: setup / loop
│   │   ├── config.h           #     Pin definitions & flight parameters
│   │   ├── imu.ino            #     IMU driver & calibration
│   │   ├── estimate.ino       #     Attitude estimation
│   │   ├── control.ino        #     PID control & motor mixing
│   │   ├── motors.ino         #     Motor / MOSFET drivers
│   │   ├── rc.ino             #     Remote control input
│   │   ├── mavlink.ino        #     MAVLink protocol
│   │   ├── wifi.ino           #     Wi-Fi & ESP-NOW communication
│   │   ├── console.ino        #     Wireless command line
│   │   ├── led.ino / power.ino / log.ino / safety.ino / time.ino / parameters.ino
│   │   └── pid.h / filter.h / quaternion.h / vector.h / util.h
│   └── gazebo/                #   Gazebo simulator (runs the same .ino sources)
│       ├── flix.world         #     Simulation world
│       ├── simulator.cpp      #     Simulator main program
│       └── models/            #     Frame / scene models
├── Docs/                      # ★ Documentation
│   ├── assembly.md            #   Assembly instructions
│   ├── usage.md               #   Build, debug, fly
│   ├── firmware.md            #   Firmware architecture (read this first)
│   ├── troubleshooting.md     #   Troubleshooting
│   ├── log.md                 #   Log analysis
│   ├── user.md                #   User builds gallery
│   └── book/                  #   mdBook flight-control textbook
├── Tools/                     # ★ Tooling
│   ├── pyflix/                #   Python flight library
│   ├── espnow-proxy/          #   ESP-NOW proxy firmware
│   ├── csv_to_ulog/           #   CSV → ULog converter
│   └── log.py / fft.py / test.py ...
└── .github/workflows/         # CI: build verification + docs + tool tests
```

**Before / after:**

| Original path | New path                               | Why                                                                        |
| ------------- | -------------------------------------- | -------------------------------------------------------------------------- |
| `flix/`       | `Firmware/flix/`                       | Arduino firmware sources                                                   |
| `gazebo/`     | `Firmware/gazebo/`                     | Simulator runs the same firmware, so it's part of the firmware project     |
| `Makefile`    | `Firmware/Makefile` (+ root forwarder) | Build script travels with the firmware; root forwarder keeps compatibility |
| `docs/`       | `Docs/`                                | Documentation                                                              |
| `tools/`      | `Tools/`                               | Tooling scripts                                                            |

## 🛩️ YaoKun S3 Edition (`yaokun-organized` branch)

> ⚠️ **This only applies to the `yaokun-organized` branch.** `master` always tracks upstream and  
> contains none of the changes below.

This branch runs Flix on the **YaoKun S3 quadcopter** and fixes the "aircraft does nothing after  
switching to our custom ESP-NOW remote" problem. The full change list, root-cause analysis and  
verification records are in [Docs/耀坤S3适配说明.md](Docs/耀坤S3适配说明.md) (Chinese).

### Key changes

| ID      | Change                                                                         | Why it is required                                                                                                                                                                                                                                                                                                                                                                |
| ------- | ------------------------------------------------------------------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **R1**  | Time base moved to a **64-bit microsecond accumulator**                        | Upstream uses `t = micros()/1e6`, and `micros()` is 32-bit → it **wraps every 71.6 minutes**. `t` jumps backwards and every `t - last >= period` gate in the project fails at once: "telemetry dies after 71.6 min, console still echoes, reboot fixes it"                                                                                                                        |
| **R2**  | `COMMAND_LONG` now **always sends an ACK**                                     | If a denied arm/mode change sends no ACK, QGC hangs on "in progress" until timeout                                                                                                                                                                                                                                                                                                |
| **R4**  | ESP-NOW RX switched to the **native `esp_now_register_recv_cb` + byte queue**  | Upstream uses the **session-based** `ESPNOWSerial::read()`, which only delivers data from already-registered peers. Peer registration happens via a 4-byte `flix` discovery packet, and upstream gates that behind `isEncrypted()` → **without encryption no session is ever created, so the remote shows "connected" while the aircraft receives nothing**. This is the core fix |
| **R5**  | Motor pins **15 / 5 / 6 / 16**, PWM 20 kHz                                     | Matches the YaoKun S3 wiring; 20 kHz is inaudible so motors do not whine                                                                                                                                                                                                                                                                                                          |
| **R6**  | Voltage sense GPIO **2**, divider **2**                                        | Measured on the YaoKun S3 board                                                                                                                                                                                                                                                                                                                                                   |
| **R7**  | IMU **autodetection** (MPU6050 on I2C / MPU-6500 on SPI) with per-bus rotation | The two boards use different IMUs. Picking the wrong model makes `IMU::create()` return `nullptr` → upstream **dereferences it and crashes**. This version adds an `imuOK` guard: arming is blocked and the LED/buzzer alarm when the IMU is unavailable                                                                                                                          |
| **R8**  | RGB LED (GPIO 41/7/14) + buzzer (GPIO 39) status system                        | Status visible without a serial console. **Connectivity is judged from `controlTime`, not `mavlinkTime`** — the latter is refreshed by *any* MAVLink frame, so it would claim "connected" with no remote at all                                                                                                                                                                   |
| **R9**  | **Fully Chinese UI** (`i18n.h`, 157 strings)                                   | Help menu, IMU calibration, init messages and errors are all Chinese                                                                                                                                                                                                                                                                                                              |
| **R10** | New parameters `IMU_AUTO_DETECT` / `IMU_ROT_LOCK` / `LED_*` / `BUZ_*`          | Changing IMU or board no longer requires recompiling                                                                                                                                                                                                                                                                                                                              |

### Bilingual UI (Chinese default, English optional)

All UI strings live in `Firmware/flix/i18n.h` and are **Chinese by default**:

```cpp
// To build the English version, add this line at the very top of flix.ino
#define FLIX_LANG_EN
```

Or pass it as a compile flag without touching any file:

```bash
arduino-cli compile --build-property "compiler.cpp.extra_flags=-DFLIX_LANG_EN" ...
```

Both language sections contain exactly the same number of strings (157:157), verified by script,  
so a string can never be added to one language only.

### Build and flash (YaoKun S3)

```bash
cd Firmware
arduino-cli compile \
  --fqbn "esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=4M,PartitionScheme=default,PSRAM=disabled" \
  --libraries "G:\Arduino\libraries" \
  --export-binaries flix
```

> 🔴 **`--export-binaries` is mandatory.** Without it the `build/` directory is not refreshed and  
> the packing script will assemble a binary that looks fine but contains stale code.


Package a single-file firmware (auto-generates the fingerprint/flashing notes):

```bash
python Firmware/tools/pack_flix_release.py --version 1.0
```

Output goes to `Firmware/release/`: `*_merged.bin` (flash at `0x0`) plus `*_指纹与烧录说明.md/.txt`.

### Four things to check for ESP-NOW

If any of these is wrong, the symptom is identical: **the remote shows "connected" but the aircraft never moves.**

1. **Same channel** — check with `ch` on the aircraft; default is **1**. The Yaokun Chinese remote (`ESP32-DJI-Dev-Yaokun20260815`) **never calls `esp_wifi_set_channel`**, so it operates on the Wi-Fi default of channel **1**, and the aircraft follows.
   > ⚠️ Earlier builds used channel 6 based on the English remote's `esp_wifi_set_channel(6)`. The aircraft listened on 6 while the remote transmitted on 1 — a complete physical-layer mismatch. **This failure is highly misleading: the remote's "peer discovery" page still lists the aircraft** (scanning is cross-channel). "Visible" does not mean "reachable".
2. **Aircraft has registered the remote as an ESP-NOW peer** — the ESP-NOW receive callback only fires for MACs present in the peer table; packets from unregistered devices are dropped in the driver. The Chinese remote sends **unicast** after `connect(mac)`, so registration is mandatory. This build handles it: broadcast peer pre-registered at boot, `onReceive` records the source MAC, and the main loop calls `learnPeerIfNeeded()` to register it (never call `esp_now_add_peer` from the callback — it takes the Wi-Fi lock and self-deadlocks).
3. **Remote is enabled** — the Chinese remote's `enabled` flag defaults to `false`. In that state it still transmits `MANUAL_CONTROL`, but all four axes sit at neutral (≈0.01). **Enable it by pressing the left (auxiliary) button on the remote** — there is no on-screen switch; it is a physical button only.
   > ⚠️ **After every aircraft reboot you must repeat: peer discovery → select the aircraft → press the left button.**
4. **Native RX path is registered** — the boot log should print the native-receive-enabled line; the `wifi` command exposes received-drop and queue-depth counters to distinguish "cannot transmit" from "cannot keep up".

**Diagnose in this order with the `wifi` command:**

| Observation | Conclusion |
| --- | --- |
| `RX packets` stays 0 | Physical layer down → check the **channel** (item 1) |
| Packets increase but parsed MAVLink frames stay 0 | Protocol mismatch |
| Frames increase but last msgid ≠ 69 | Remote is not sending `MANUAL_CONTROL` |
| msgid = 69 yet `rc` shows 0.01 on all axes | **Remote not enabled** → press the left button (item 3) |
| Everything above is fine and the axes follow the sticks | ✅ Link is healthy |

> ⚠️ A home router on the **same channel** as the aircraft AP causes interference (phone connects then drops, UI appears frozen). Change the aircraft side with `ch <1-13>`, but **the remote must be changed too**, otherwise the physical layer simply receives nothing.

### Flight envelope (read this first)

This firmware is **attitude stabilization plus manual throttle only**. It has no position or velocity  
sensors (no GPS / barometer / rangefinder / optical flow):

- **Releasing the sticks does not hover** — attitude levels out, but horizontal velocity is retained and the aircraft keeps drifting.
- **Releasing the throttle makes it descend** — there is no altitude hold.
- **Full silence on all sources trips the failsafe** — 1 s of no control input ramps the throttle down; 10 s zeroes it and disarms.

Fly first in an open area, low, with prop guards installed.

## 🚀 Getting started

### 1. Build the firmware

Install [arduino-cli](https://arduino.github.io/arduino-cli/latest/installation/) first.

```bash
git clone https://github.com/YaoKunS/flix.git
cd flix
git checkout yaokun-organized   # ★ switch to the reorganized branch (default master mirrors upstream)

# First run auto-downloads ESP32 core 3.3.10 and deps (FlixPeriph / MAVLink)
make                                    # default: ESP32
make BOARD=esp32:esp32:esp32s3          # ESP32-S3
make BOARD=esp32:esp32:esp32c3          # ESP32-C3
```

> 💡 `make` works from the repo root — the root `Makefile` forwards to `Firmware/`.  
> `cd Firmware && make` works identically.

### 2. Flash and monitor

```bash
make upload                 # build and flash
make monitor                # serial monitor at 115200
make erase                  # erase bootloader
```

### 3. Run the Gazebo simulator

Play with it before buying any hardware (needs Gazebo 11):

```bash
make build_simulator
make simulator
```

### 4. Wireless control

Build and flash the ESP-NOW proxy:

```bash
make upload_proxy
```

Join the drone's Wi-Fi (AP name is printed in the serial log) and control it from a  
browser or a ground station.

## 🔌 Bill of materials

| Type                                                 | Part                                                                                                                                                                                                                                              |                                                                         Image                                                                         | Quantity |
| ---------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :---------------------------------------------------------------------------------------------------------------------------------------------------: | :------: |
| MCU board                                            | ESP32 Mini.<br />ESP32-S3 / ESP32-C3 boards also supported.                                                                                                                                                                                       |                                                        <img src="Docs/img/esp32.jpg" width=100>                                                       |     1    |
| IMU (and barometer¹)                                 | GY-91, MPU-9265 (or other MPU-9250 / MPU-6500)<br />ICM20948V2<br />GY-521 (MPU-6050)                                                                                                                                                             | <img src="Docs/img/gy-91.jpg" width=90 align=center><br /><img src="Docs/img/icm-20948.jpg" width=100><br /><img src="Docs/img/gy-521.jpg" width=100> |     1    |
| *Boost converter (optional, more stable power)*      | *5V output*                                                                                                                                                                                                                                       |                                                     <img src="Docs/img/buck-boost.jpg" width=100>                                                     |     1    |
| Motor                                                | 8520 3.7V brushed motor.<br />Exact 3.7V rated motor is needed, not ranged voltage (3.7V — 6V).<br />Match motor shaft and propeller hole diameters!                                                                                              |                                                       <img src="Docs/img/motor.jpeg" width=100>                                                       |     4    |
| Propeller                                            | 55 mm or 65 mm                                                                                                                                                                                                                                    |                                                        <img src="Docs/img/prop.jpg" width=100>                                                        |     4    |
| MOSFET (transistor)                                  | UMW 100N03A or [analog](https://t.me/opensourcequadcopter/33).<br />Warning: don't use KIA 100N03A or other manufacturers, they might not work!                                                                                                   |                                                       <img src="Docs/img/100n03a.jpg" width=100>                                                      |     4    |
| Pull-down resistor<br />Voltage measurement resistor | 10 kΩ                                                                                                                                                                                                                                             |                                                     <img src="Docs/img/resistor10k.jpg" width=100>                                                    |     6    |
| 3.7V Li-Po battery                                   | LW 952540 (or any compatible by size).<br />25C with 1000 mAh or more is recommended!                                                                                                                                                             |                                                       <img src="Docs/img/battery.jpg" width=100>                                                      |     1    |
| Battery connector cable                              | MX2.0 2P female                                                                                                                                                                                                                                   |                                                         <img src="Docs/img/mx.png" width=100>                                                         |     1    |
| Li-Po battery charger                                | Any                                                                                                                                                                                                                                               |                                                       <img src="Docs/img/charger.jpg" width=100>                                                      |     1    |
| Screws for IMU board                                 | M3x5                                                                                                                                                                                                                                              |                                                      <img src="Docs/img/screw-m3.jpg" width=100>                                                      |     2    |
| Screws for frame assembly                            | M1.4x5                                                                                                                                                                                                                                            |                                               <img src="Docs/img/screw-m1.4.jpg" height=30 align=center>                                              |     4    |
| Frame main part                                      | 3D printed²: [`stl`](Docs/assets/flix-frame-1.1.stl) [`step`](Docs/assets/flix-frame-1.1.step)<br />Recommended: layer 0.2 mm, line 0.4 mm, infill 100%.                                                                                          |                                                       <img src="Docs/img/frame1.jpg" width=100>                                                       |     1    |
| Frame top part                                       | 3D printed: [`stl`](Docs/assets/esp32-holder.stl) [`step`](Docs/assets/esp32-holder.step)                                                                                                                                                         |                                                    <img src="Docs/img/esp32-holder.jpg" width=100>                                                    |     1    |
| Washer for IMU board                                 | 3D printed: [`stl`](Docs/assets/washer-m3.stl) [`step`](Docs/assets/washer-m3.step)                                                                                                                                                               |                                                      <img src="Docs/img/washer-m3.jpg" width=100>                                                     |     2    |
| Transmitter (recommended)                            | CC2500, like BetaFPV LiteRadio CC2500 (RC receiver / Wi-Fi).<br />Two-sticks gamepad (Wi-Fi only) — see [supported joysticks](https://docs.qgroundcontrol.com/master/en/qgc-user-guide/setup_view/joystick.html#supported-joysticks).<br />Other⁵ |                                  <img src="Docs/img/betafpv.jpg" width=100><img src="Docs/img/logitech.jpg" width=80>                                 |     1    |
| *RC receiver (optional)*                             | *DF500 or other³*                                                                                                                                                                                                                                 |                                                         <img src="Docs/img/rx.jpg" width=100>                                                         |     1    |
| Wires                                                | 28 AWG recommended                                                                                                                                                                                                                                |                                                     <img src="Docs/img/wire-28awg.jpg" width=100>                                                     |          |
| Tape, double-sided tape                              |                                                                                                                                                                                                                                                   |                                                                                                                                                       |          |

*¹ — barometer is not used for now.*  
  
*² — this frame is optimized for the GY-91 board; with others, change the mounting holes.*  
  
*³ — you may also use any transmitter-receiver pair with an SBUS interface.*

Tools required:

- 3D printer.
- Soldering iron.
- Solder wire (with flux).
- Screwdrivers.
- Multimeter.

Feel free to modify the design and/or the code and build your own improved version.  
Send your results to the [official Telegram chat](https://t.me/opensourcequadcopterchat),  
or directly to the author ([E-mail](mailto:okalachev@gmail.com) · [Telegram](https://t.me/okalachev)).

## 🔧 Schematics

### Simplified connection diagram

![Flix version 1 schematics](Docs/img/schematics1.svg)

*(Dashed elements are optional.)*

Motor connection scheme:

![MOSFET connection scheme](Docs/img/mosfet-connection.png)

A user-contributed [variant of the complete circuit diagram](https://miro.com/app/board/uXjVN-dTjoo=/?moveToWidget=3458764612338222067\&cot=14) is also available.

### Notes

- Power the ESP32 Mini from the Li-Po battery via VCC (+) and GND (−).
- Connect the IMU board over VSPI, power it from 3.3V and GND:
  | IMU pin      | ESP32 pin    |
  | ------------ | ------------ |
  | GND          | GND          |
  | 3.3V         | 3.3V         |
  | SCL *(SCK)*  | SVP (GPIO18) |
  | SDA *(MOSI)* | GPIO23       |
  | SAO *(MISO)* | GPIO19       |
  | NCS          | GPIO5        |
- Solder pull-down resistors to the MOSFETs.
- Connect motors to the ESP32 Mini through MOSFETs:
  | Motor   | Position    | Direction         | Prop type | Motor wires   | GPIO           |
  | ------- | ----------- | ----------------- | --------- | ------------- | -------------- |
  | Motor 0 | Rear left   | Counter-clockwise | B         | Black & White | GPIO12 *(TDI)* |
  | Motor 1 | Rear right  | Clockwise         | A         | Blue & Red    | GPIO13 *(TCK)* |
  | Motor 2 | Front right | Counter-clockwise | B         | Black & White | GPIO14 *(TMS)* |
  | Motor 3 | Front left  | Clockwise         | A         | Blue & Red    | GPIO15 *(TD0)* |
  Clockwise motors have blue & red wires and correspond to propeller type A.  
  Counter-clockwise motors have black & white wires and correspond to propeller type B.
- Optionally connect an RC receiver to the ESP32's UART2:
  | Receiver pin | ESP32 pin                               |
  | ------------ | --------------------------------------- |
  | GND          | GND                                     |
  | VIN          | VCC (or 3.3V depending on the receiver) |
  | Signal (TX)  | GPIO4                                   |
- Optionally connect the battery voltage divider to any ADC1 pin (e.g. *GPIO32* on ESP32, *GPIO3* on ESP32-S3).

  ESP32 and ESP32-S3 [can measure](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/adc.html#analogsetattenuation) up to 3.1 V,  
  while ESP32-S3 / ESP32-C3 go up to 2.5 V — choose divider resistors accordingly.

## 📚 Recommended reading order

If you're new to flight control, **read in this order — half a day to grasp the principles**:

1. [Firmware architecture](Docs/firmware.md) — how data flows and how control loops close
2. [Assembly](Docs/assembly.md) — build the machine
3. [Usage](Docs/usage.md) — build, debug, fly
4. [Textbook](Docs/book/) — attitude estimation, frames, gyroscopes
5. [Troubleshooting](Docs/troubleshooting.md) — check here when takeoff misbehaves
6. [Log analysis](Docs/log.md) — inspect flights with PlotJuggler

More:

- [Simulator](Firmware/gazebo/README.md)
- [Python library](Tools/pyflix/README.md)
- [User builds gallery](Docs/user.md)

## 🔌 Community

- Development channel (Russian): <https://t.me/opensourcequadcopter>
- Official Telegram chat (EN / RU): <https://t.me/opensourcequadcopterchat>
- Development write-up (Russian): <https://habr.com/ru/articles/814127/>
- Textbook site: <https://quadcopter.dev>

## 📜 License & credits

- **Upstream project**: Flix © [Oleg Kalachev](https://github.com/okalachev) ·  
  source at <https://github.com/okalachev/flix>
- **This fork**: Chinese documentation and directory reorganization only.  
  **No firmware logic was modified.** All rights to the firmware remain with the original author.
- **Fork maintained by**: YaoKunSmart · <https://github.com/YaoKunS>
- **Bilibili**: <https://space.bilibili.com/3707011699050767>

> ⚠️ **Important note about licensing**:  
> The upstream [okalachev/flix](https://github.com/okalachev/flix) repository ships **no LICENSE file**  
> and does not name a license in its README. Strictly speaking this means **copyright is retained  
> by the author** (all rights reserved by default).  
> This fork therefore **deliberately does not add a LICENSE file** — choosing a license is the  
> original author's decision, not a forker's.  
> This repository exists for study, research and technical exchange. For commercial use or  
> redistribution, obtain explicit authorization from the author first and comply with local law.

> ⚠️ When using, modifying or redistributing this project, please keep the original author's  
> attribution and project source.

## ⚠️ Disclaimer

This is a DIY project, and I hope you find it interesting and useful. However, it's not easy  
to assemble and set up, and it's provided "as is" without any warranties. There's no guarantee  
that it will work perfectly, or even work at all.

⚠️ The author is not responsible for any damage, injury, or loss resulting from the use of this  
project. Use at your own risk!

---

<div align="center">

**🚁 Flix** — created by [Oleg Kalachev](https://github.com/okalachev)  
· Chinese fork maintained by [YaoKunSmart](https://github.com/YaoKunS)

</div>
