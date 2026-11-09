# MildlySpicyDragon 2.0 / Flix Organized Edition — Firmware Fingerprint & Flashing Guide

## 1. What this firmware is

An **organized edition of the open-source flight controller Flix** (original author Oleg Kalachev), adapted for the **MildlySpicyDragon 2.0** ESP32 mini drone.
The same source tree supports both the YaoKun S3 board and the MildlySpicyDragon 2.0 through a **compile-time board macro** (`FLIX_BOARD=2`).

- Upstream: <https://github.com/okalachev/flix> — `(C) Oleg Kalachev` (no standalone LICENSE file; copyright stays with the author)
- Organized-edition repo: <https://github.com/YaoKunS/flix> (branch `yaokun-organized`)
- Hardware design: MildlySpicyDragon (malagis) — <https://oshwhub.com/malagis/esp32-mini-plane>
- **Firmware maintenance:** the MildlySpicyDragon 2.0 port (board adaptation and maintenance) is **maintained by YaoKunSmart**. Hardware copyright belongs to the dragon-fruit author; kernel copyright belongs to Oleg Kalachev. The two brands are independent.

> ★ Flight model: **attitude self-leveling + manual throttle** only. No GPS / barometer / rangefinder / optical flow. Centering the sticks does **not** hover; releasing throttle **will** drop altitude.

## 2. Version & build

| Item | Value |
| --- | --- |
| Firmware version | **v1.2** (organized edition, MildlySpicyDragon 2.0) |
| Build time | Nov  9 2026 21:45:07 |
| Chip | ESP32（经典 Xtensa LX6，双核 240MHz） |
| Flash | 4 MB (full image padded to 4 MB) |
| Build flags | `esp32:esp32:esp32:FlashSize=4M,PartitionScheme=default` + `-DFLIX_BOARD=2` |
| Application size | 1,065,664 B (0x1042C0) |
| Full image size | 4,194,304 B (0x400000) |
| Full image SHA-256 | `EC6B83B15B6771A15C38D6DD64CC080FD18503BF08C0FB9DF58C3E02695F8465` |

> The `version` field inside `esp_app_desc_t` (at `0x10020`) is written by the precompiled ESP32 core and is **not** this project's version. Trust the boot banner: `Firmware version: v1.2 (YaoKun organized edition, MildlySpicyDragon 2.0)`.

## 3. Pinout

| Function | Pin |
| --- | --- |
| Motors (PWM) | 26 / 27 / 33 / 32（依次 RL / RR / FR / FL） |
| IMU (I2C) | SDA 21 / SCL 22（I2C0） |
| IMU (SPI) | MOSI 23 / MISO 19 / SCLK 18 / CS 5（SPI） |
| Buzzer | 25 |
| Status LED | 2 |
| Battery voltage | 35（电池电压检测，ADC1_CH7） |
| SBus receiver | 4（SBus 接收机） |
| Optical flow (reserved) | 13 / 14 / 12 / 15（光流预留） |
| Laser rangefinder (reserved) | SDA 16 / SCL 17（I2C1，激光测距预留） |

> Pins live in `Firmware/flix/board.h`, branch `BOARD_HLG_V2`. **Do not reuse the S3 pinout** — the motor pins are completely different.

## 4. Flashing (single file)

### Option A: Flash Download Tool (GUI)

1. Select chip **ESP32** (⚠️ not ESP32-S3 — this is the classic ESP32).
2. Develop mode, add only this file at address **`0x0`**: `MildlySpicyDragon_火龙果2.0_Flix整理版_v1.2_20261109_4MB.bin`
3. SPI SPEED **40MHz**, SPI MODE **DIO**, FLASH SIZE **4MB**, baud 460800.
4. ⚠️ **Tick DoNotChgBin.** The image header carries SPI mode/speed/size plus the appended SHA-256; rewriting the header breaks boot validation.

### Option B: esptool

```bash
esptool --chip esp32 --port COMx --baud 460800 write_flash 0x0 "MildlySpicyDragon_火龙果2.0_Flix整理版_v1.2_20261109_4MB.bin"
```

### 🔴 Segment addresses (advanced)

| Segment | Address | Note |
| --- | --- | --- |
| bootloader | **`0x1000`** | 🔴 Classic ESP32 uses **0x1000**, NOT 0x0 |
| partition table | `0x8000` | default 4MB layout |
| OTA data init | `0xE000` | boot_app0.bin |
| application | `0x10000` | app-only update keeps all NVS settings |

> 🔴 Most common mistake: flashing the bootloader at `0x0` (the ESP32-S3 rule). The classic ESP32 ROM looks for the `0xE9` magic at `0x1000`; you get a flash that reports *verified* but never boots (0 bytes on serial, stuck at `waiting for download`).

## 5. Verify what you flashed

1. Open the port at **115200 8N1** and press RST / power-cycle.
2. The boot banner should read:

```
==============================================================
  MildlySpicyDragon 2.0 Flight Controller (Flix YaoKun Edition)
  2025 Spark Program Award Project - ESP32 Mini Drone - ESP32 DevKit V1
  Open hardware: https://oshwhub.com/malagis/esp32-mini-plane
  Bilibili: https://space.bilibili.com/544479100
--------------------------------------------------------------
  Firmware: https://github.com/YaoKunS/flix
--------------------------------------------------------------
  Kernel (C) Oleg Kalachev  https://github.com/okalachev/flix
  Hardware design: MildlySpicyDragon (malagis)
  Firmware port maintained by YaoKunSmart
  Taobao (YaoKunSmart): https://shop35432590.taobao.com/
==============================================================
```

3. Then `Firmware version: v1.2 (YaoKun organized edition, MildlySpicyDragon 2.0)`.
4. SHA-256 check (Windows): `certutil -hashfile "MildlySpicyDragon_火龙果2.0_Flix整理版_v1.2_20261109_4MB.bin" SHA256`

## 6. Wi-Fi hotspot

SSID = `YaoKun_` + last two bytes of the factory STA MAC (e.g. `YaoKun_2DD4`). Always trust the `当前热点名：` line in the boot log.

Factory password: **`1-2-3-4-5-6-7-8`** — the hyphens are separators only, type the eight characters one by one (digits one through eight).

The hotspot serves **MAVLink on UDP 14550 only** — there is **no web page**. A blank `192.168.4.1` is expected; check `已连设备数` from the `wifi` command instead.

## 7. Radio control

- **ESP-NOW**: YaoKun RC, fixed channel **1**.
- 🔴 The Chinese RC ships **disabled** — press the left button once after connecting. This must be repeated after every aircraft reboot (anti-mistouch design, not a bug).
- **SBus**: wire to GPIO `4（SBus 接收机）`. Run `cr` to calibrate on first use.
- 🔴 Calibrations are **not interchangeable** between different transmitters.

## 8. Links

- Bilibili: <https://space.bilibili.com/3707011699050767>
- OSHWHub: <https://oshwhub.com/lindahe>
- GitHub: <https://github.com/YaoKunS>
- Taobao: <https://shop35432590.taobao.com/>
- Hardware author: MildlySpicyDragon (malagis) — <https://oshwhub.com/malagis/esp32-mini-plane>

---

Generated by `Firmware/tools/gen_hlg_note.py` (20261109). Hashes, sizes, layout and build time are read from the binary itself.
