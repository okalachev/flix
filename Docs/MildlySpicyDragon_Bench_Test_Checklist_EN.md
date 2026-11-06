# MildlySpicyDragon 2.0 · Flix YaoKun Organised Edition v1.1 — Bench Test Checklist

> Goal: verify four things — "it flashes and boots", "the IMU is auto-detected correctly",
> "both RC links (ESP-NOW / SBUS) work", "motor positions map correctly".
> **Props off for the entire procedure.** Passing this checklist is the gate for shipping a board / pushing to
> the `yaokun-organized` branch.
>
> ⚠️ **This is a classic ESP32 (not an S3)**: flashing addresses, USB-serial chip, pinout and IMU bus
> all differ from the YaoKun S3 board. **Do not reuse the S3 checklist or pin table** — wrong motor pins
> mean all four motors spin up out of order at power-on.
> 🌐 中文版：[火龙果版台架实测清单V1.1.md](火龙果版台架实测清单V1.1.md) ·
> Sister checklist for the YaoKun S3 board: [Bench_Test_Checklist_EN.md](Bench_Test_Checklist_EN.md)
> 🖨️ Illustrated HTML (open in a browser / print): [MildlySpicyDragon_Bench_Test_Checklist_EN.html](MildlySpicyDragon_Bench_Test_Checklist_EN.html)
>
> Pin definitions live in `Firmware/flix/board.h` (`BOARD_HLG_V2` branch); if this document ever disagrees
> with the source, the source wins.

---

## ✅ Test status (2026-11-06, COM30 on real hardware + serial capture)

| Item | Status | Result |
| --- | --- | --- |
| Full-image flash | ✅ | `MildlySpicyDragon_…_v1.1_20261106_整片4MB.bin` (SHA-256 `A3738D46…DC71A413`), bootloader checksum passed, boots normally |
| Chinese boot banner | ✅ | All three fingerprint lines + OSHWHub / Bilibili / Taobao lines complete |
| IMU auto-detection | ✅ | I2C socket empty → automatic fallback to SPI → **MPU-6500 detected** (SCK=18 MISO=19 MOSI=23 CS=5, orientation offset −1.5708) |
| Voltage sense parameters | ✅ | Boot log: `battery sense: GPIO35, divider 2` |
| Hotspot name | ✅ | `current SSID: YaoKun_2DD4` (chip MAC `f4:65:0b:49:2d:d4`) |
| ESP-NOW ready | ✅ | `native ESP-NOW receiver ready`, `channel: 1` |
| ESP-NOW packets / throttle range | ⬜ TODO | Needs the Chinese remote — section 5 |
| SBUS RC (GPIO4) | ⬜ TODO | Needs an SBUS receiver — section 6 |
| Motor mapping / arming | ⬜ TODO | Section 7 (**props off**) |
| RC-loss failsafe | ⬜ TODO | Section 8 |
| Long-run wrap-around test | ⬜ TODO | Continuous run > 72 minutes |

---

## ⚠️ Key differences vs the YaoKun S3 board (read this first)

| Item | MildlySpicyDragon 2.0 | YaoKun S3 |
| --- | --- | --- |
| Chip | classic ESP32 (DevKit V1, 30-pin) | ESP32-S3 |
| Flash / partition scheme | 4 MB / `default` | 8 MB / `default_8MB` |
| bootloader address | **`0x1000`** (classic-ESP32 rule) | `0x0` |
| app address | `0x10000` | `0x10000` |
| Build flags | `esp32:esp32:esp32:FlashSize=4M,PartitionScheme=default` + `-DFLIX_BOARD=2` | `esp32s3` + `8M,default_8MB` (default board, no macro) |
| Serial link | USB-UART bridge (virtual COM) | native USB-CDC (HWCDC) |
| Motor GPIOs | RL=26 / RR=27 / FR=33 / FL=32 | RL=15 / RR=5 / FR=6 / FL=16 |
| IMU | socketed: MPU6500 on **SPI** (18/19/23/CS5) or MPU6050 on **I2C** (SDA21/SCL22) | on-board MPU6500 (SPI, CS=17/42) or MPU6050 (I2C, SDA9/SCL8) |
| LED | **single LED GPIO2** (three channels OR-ed into one lamp, state shown by blink rate) | RGB 41/7/14 |
| Voltage ADC | GPIO35 (input-only, ADC1_CH7), divider ratio 2 | GPIO2 |
| SBUS | **GPIO4, enabled by default at the factory** | GPIO1, disabled by default (`p RC_RX_PIN 1` to enable) |
| Buzzer | GPIO25 | GPIO39 |

---

## 0. Preparation

| Item | Detail |
| --- | --- |
| Firmware | `release固件v1.1/MildlySpicyDragon_火龙果2.0_Flix整理版_v1.1_20261106_整片4MB.bin` (SHA-256 and fingerprints in the flashing guide shipped alongside) |
| Serial | virtual COM port from the USB-UART bridge, 115200. **Do not toggle DTR/RTS manually in a serial tool** — on some USB-UART chips `DTR=1,RTS=0` is exactly "hold reset" and the board will sit there doing nothing |
| Remote | either the Chinese ESP-NOW remote, or an SBUS receiver wired to GPIO4 |
| Other | **the other aircraft powered off** (same-band interference ruins attribution); **props off** |

### Flashing quick reference

**Option A: Flash Download Tool (GUI, recommended)** — chip **ESP32** (⚠️ not ESP32-S3), mode Develop,
add only the full-image bin, address **`0x0`**.

**Option B: esptool**

```bash
# Full image (wipes NVS calibration / parameters — new board or intentional reset)
esptool --port COMx --baud 460800 --before default-reset --after hard-reset \
  write-flash 0x0 MildlySpicyDragon_火龙果2.0_Flix整理版_v1.1_20261106_整片4MB.bin

# app region only (0x10000) — keeps NVS calibration, SSID and channel (normal upgrade path)
esptool --port COMx --baud 460800 --before default-reset --after hard-reset \
  write-flash 0x10000 flix.ino.bin
```

> 🔴 When flashing the bootloader on its own, the address is `0x1000`, not `0x0` as on the S3
> (copying the S3 address gives "verified" but the chip never boots).
> The board often stays in download mode afterwards: `esptool --after watchdog-reset run`, or replug USB.

---

## 1. Boot banner (three fingerprint lines)

```
MildlySpicyDragon 2.0 (based on Flix YaoKun edition) booting...
MildlySpicyDragon | 微辣火龙果 | 2025星火计划获奖项目 ESP32迷你无人机 | Flix
Firmware version: v1.1 (YaoKun organized edition, MildlySpicyDragon 2.0)
```

> The default build is the **Chinese** UI. Switching to English requires `-DFLIX_LANG_EN` at compile time;
> the fingerprint and version lines above are the English variants.

- [ ] All three lines present (the second half of the banner must show OSHWHub / Bilibili /
      **Taobao (YaoKunSmart)** — that is the MildlySpicyDragon-specific banner. Seeing the YaoKun S3 banner
      means the wrong firmware was flashed)
- [ ] `channel: 1`
- [ ] `native ESP-NOW receiver ready`
- [ ] `battery sense: GPIO35, divider 2`
- [ ] `current SSID: YaoKun_2DD4` (prefix `YaoKun_` + last two MAC bytes; this board's MAC ends `2d:d4`)

### Hotspot quick reference

| Item | Value |
| --- | --- |
| Prefix | `YaoKun_` |
| Suffix | last two bytes of the factory STA MAC; this board = `YaoKun_2DD4` |
| Default password | **`1-2-3-4-5-6-7-8`** (hyphens are separators only — type the eight digits; digit by digit: one, two, three, four, five, six, seven, eight) |
| Rename | `ap <ssid> <password>` + `reboot` (⚠️ the angle brackets are placeholders — never type the literal words `<ssid>`, or NVS stores the placeholder and auto-naming stops working) |

---

## 2. IMU auto-detection (`imu`)

The firmware probes **I2C (socket) first, then SPI**. Real capture, 2026-11-06 (empty I2C socket, SPI module fitted):

```
auto-detecting IMU...
detected MPU6050 family (I2C) SDA=21 SCL=22
  WHO_AM_I = 0xFF
  mounting offset: +1.5708 rad about Z  [I2C id]
Incorrect device ID: 0x00
first attempt failed (model 3 / total 1 / status 1), retrying with other models
detected MPU6500 family (SPI) SCK=18 MISO=19 MOSI=23 CS=5
  mounting offset: -1.5708 rad about Z  [fallback]
IMU: MPU-6500 detected
IMU ready: MPU-6500, bus SPI
```

**Interpretation**

| Module fitted | Expected detection | Bus / pins |
| --- | --- | --- |
| MPU6050 (socket) | `MPU6050 … I2C0`, WHO=0x68 | SDA=21 SCL=22 |
| MPU6500 (socket) | `MPU-6500 … SPI`, WHO=0x70 | SCK=18 MISO=19 MOSI=23 CS=5 |

- [ ] `imu` model / WHO_AM_I / bus match the physical module
- [ ] **Lower the right side → `st` roll is positive** (the only orientation criterion; I2C offset +1.5708,
      SPI offset −1.5708, selected automatically — do not hand-edit)
- [ ] Level and still: roll / pitch within ±2°
- [ ] Chip temperature is a real value (≈40 °C; 0.0 means no communication)
- [ ] After swapping IMU modules, **re-run `ca`** (bias differs per module)

---

## 3. Voltage sensing (`pw`)

- [ ] `pw` voltage matches a multimeter (±0.05 V; divider ratio 2 is already configured for GPIO35 with
      100K+100K or 10K+10K)
- [ ] Unplugging the battery drops the reading to ~0 (not frozen at the last value)

> 🔴 GPIO35 is an **input-only ADC1** pin. ADC2 is mutually exclusive with Wi-Fi (this firmware keeps the
> hotspot on), so a voltage sense wired to ADC2 reads a constant 0 ⇒ false low-battery ⇒ the buzzer screams
> forever. The board definition pins it to ADC1 for exactly this reason.

---

## 4. Wi-Fi mode (`wifi`)

- [ ] Default `WIFI_MODE=4` (AP + ESP-NOW together): the remote can reach it over ESP-NOW *and* the hotspot
      `YaoKun_2DD4` is visible
- [ ] `wifi espnow` / `wifi ap` switches to a pure mode and the choice survives a reboot

---

## 5. ESP-NOW RC (Chinese remote)

Same procedure as the S3 board:

1. Power on the remote → open the "peer discovery" page → **select** the aircraft to connect
2. **Press the "left button" (aux button) to enable control output** — ⚠️ there is no on-screen switch,
   it is a physical button: press once to enable, again to disable
3. ⚠️ **After the aircraft reboots you must reconnect and re-enable** (the ESP-NOW session is gone)

```
> wifi
```

| Symptom | Conclusion |
| --- | --- |
| `packets received` = 0 | physical link dead → check channel (must be 1) and whether the remote is enabled |
| packets > 0, frames = 0 | bytes arrive but no MAVLink frame → remote is in Raw protocol mode |
| frames > 0, msgid ≠ 69 | remote only sends HEARTBEAT (not enabled / not on the control page) |
| msgid = 69, rc all zero | left button not pressed (not enabled) |

- [ ] Packet count / parsed frame count keep increasing
- [ ] `last msg ID: 69 (MANUAL_CONTROL)`, `source sysid: 127`
- [ ] Throttle range: minimum ≈ **−1.0** / centre ≈ **0.0** / maximum ≈ **+1.0**
      (❌ if you only get `0 / +0.5 / +1.0`, **do not fly**)

---

## 6. SBUS RC (MildlySpicyDragon-specific, GPIO4 enabled by default)

> On this board SBUS is enabled at the factory (`PIN_RC_RX=4`) — unlike the S3, which needs `p RC_RX_PIN 1`.
> SBUS and ESP-NOW do not mix into one value stream: if an SBUS receiver is present, SBUS drives the controls;
> the ESP-NOW `MANUAL_CONTROL` path is handled separately.

- [ ] Receiver signal wire to **GPIO4** (+ 3.3 V/5 V and GND)
- [ ] `rc` shows channel values moving with stick input (with no receiver attached, `rc` prints the SBUS pin
      number and the values stay put — that is normal)
- [ ] **`cr` RC calibration** (8 Chinese prompts, 3 s each): throttle / yaw / pitch / roll / mode switch in
      order as prompted — **the mode switch must be held at each of its two extreme positions** during its step,
      otherwise the channel is not detected
- [ ] Calibration ends with "detected N / 5" — only N = 5 passes
- [ ] `rc` shows distinct values for each of the three mode-switch positions
- [ ] Throttle range: down ≈ −1.0 side, up ≈ +1.0 side (check with `rc` after `cr`)
- [ ] Arming / disarming via the remote switch works

> 🔴 **Calibration from a different transmitter must not be reused** (full-stick values differ) — re-run `cr`
> whenever you change the transmitter.
> 🔴 **Never use `preset` to wipe parameters** (it takes IMU calibration with it); re-run `cr` to reset RC only.

---

## 7. Motors and arming (★props off)

```
> mfr 0.1   → front-right (GPIO33)   > mfl 0.1  → front-left  (GPIO32)
> mrr 0.1   → rear-right  (GPIO27)   > mrl 0.1  → rear-left   (GPIO26)
```

- [ ] All four motors spin, **in the right positions** (`mfr` must spin the front-right one; cross-check GPIO)
- [ ] Directions match a quad-X layout
- [ ] `mot` shows all four at 0 with the throttle down

> 🔴 **"Watch the battery voltage sag to tell whether a motor spins" is a false criterion** — on the bench with
> USB plugged in the motors are fed from USB 5 V via the LDO, so the battery terminal does not sag.
> Real criteria: listen / look / read `mot`. To check current draw, unplug USB and run on battery only.

Arming:
- [ ] Throttle to minimum → `arm` or the remote switch → `rc` shows armed = 1
- [ ] Three short beeps (GPIO25 active buzzer)
- [ ] Arming with the throttle not at minimum → prints "arming refused: throttle not at minimum"

---

## 8. RC-loss failsafe

Firmware logic (`safety.ino`): while armed, **no control input for 1 second** → smooth descent (AUTO mode,
throttle ramps down); **10 seconds later the throttle hits 0 and the aircraft disarms**.

- [ ] Cut power to the remote (or switch the receiver off)
- [ ] After ~1 s the aircraft enters the descent logic (buzzer sounds; the single LED switches to the
      corresponding blink pattern — on this board state is conveyed by blink rate only, so record the actual
      pattern back into this table once measured)
- [ ] After ~10 s the throttle reaches 0 and the aircraft disarms

> ⚠️ Test both links: pull the SBUS wire when SBUS is in use, and switch the remote off when ESP-NOW is in use
> (`controlTime` is refreshed by whichever link is actually active).

---

## 9. Long-run test (proof that the R1 wrap-around fix holds)

```
> time
```

- [ ] Continuous run **> 72 minutes**: telemetry never stops, `uptime` increases monotonically
- [ ] Even if `microsecond counter wrap count` > 0, `uptime` **never goes backwards**

> The old build died every 71.58 minutes (telemetry stopped, console still echoed). This item is the final
> proof that the fix works.

---

## 10. "Fake bugs" hit during bench testing (do not re-investigate)

| Symptom | Truth |
| --- | --- |
| `sample rate` shows 20 Hz / 1 Hz | it is **`loopRate` (main-loop frequency)**, not the IMU ODR. Normal during calibration and idle — **do not touch `configureIMU()`** |
| Boot log reports an I2C failure, then succeeds on SPI | **normal fallback**: empty I2C socket reads WHO 0xFF, so it probes SPI and hits. Not a fault |
| `mot` values asymmetric, containing 0 and 1 | `desaturate()` **clipping** — the four channels are no longer comparable. Verify mixer signs offline with `Firmware/tools/mixer_sign_check.py` |
| Attitude is nonsense after `ca` | `ca` runs 48 s with no input waits; a human cannot flip six faces in time, so the scale collapses. Rescue: `p IMU_ACC_SCALE_X/Y/Z 1` + `p IMU_ACC_BIAS_X/Y/Z 0`; `st` should return to ±2° |
| Bench: "motors spin but battery voltage does not drop" | with USB plugged in the motors run off USB 5 V; the battery terminal genuinely does not sag (see section 7) |
| Serial port will not open / board silent | first make sure nobody manually pulled DTR/RTS (that can hold the chip in reset); then check the USB-UART driver |

---

## 11. Troubleshooting

| Symptom | Check first |
| --- | --- |
| Flash reports "verified" but the board never boots | chip selected as ESP32-S3 by mistake? bootloader flashed at 0x0 instead of 0x1000? Flashing the full image at 0x0 avoids both |
| Banner shows YaoKun S3 content | wrong firmware — reflash the MildlySpicyDragon full image |
| No RC response (ESP-NOW) | `packets received` in `wifi`; 0 → channel (must be 1), remote enable state |
| Remote says connected but the aircraft does not move | **press the left button** (`enabled` defaults to false) |
| SBUS sticks do nothing | check `rc`; if uncalibrated re-run `cr`; check the GPIO4 wiring |
| Mode switch does not change flight mode | `cr` step 8 did not see both extreme positions → re-run `cr` |
| Motors do not spin | rule out the section-7 false criterion, then test each motor with `mfr 0.1`; if none spin check arm state |
| IMU not detected | follow the table in section 2 (I2C=21/22, SPI=18/19/23/CS5); seat the header fully |
| Voltage stuck at 0 | wiring must land on GPIO35; see the ADC1 note in section 3 |
| UI comes up in English | English build or upstream original — check the first banner line |

---

## 12. After passing

Everything ticked → clear the board for shipping. If code or docs changed → push to
`github.com/YaoKunS/flix`, branch `yaokun-organized`.

Any ❌ → send me the raw serial output for that item and I will localise it before changing anything.
