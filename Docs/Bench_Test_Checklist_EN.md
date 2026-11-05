# YaoKun S3 · Flix Organized Edition v1.1 — Bench Test Checklist (follow along after wiring)

> Purpose: verify four things — "the ESP-NOW remote actually works", "the time base never wraps",
> "the IMU is identified correctly", "the Chinese console UI is complete".
> **Remove the propellers for the entire procedure.** Passing this checklist is the gate for pushing to the
> `yaokun-organized` branch.
>
> 🌐 中文版：[台架实测清单_v1.1.md](台架实测清单_v1.1.md)

---

## ✅ Test progress (2026-10-18)

| Item | Status | Result |
| --- | --- | --- |
| Flashing (single-file firmware) | ✅ | Both aircraft reported `Hash of data verified` |
| Boot banner in Chinese | ✅ | All three fingerprint lines present |
| IMU detection · aircraft #1 | ✅ | MPU-6500 / SPI / WHO=0x70 / CS=17 / orientation −π/2 |
| IMU detection · aircraft #2 | ✅ | MPU6050 / I2C0 / WHO=0x68 / SDA=9 SCL=8 / orientation +π/2 |
| Old/new PCB auto-compatibility | ✅ | Same firmware, the CS candidate list hit 17 automatically |
| ESP-NOW reception | ✅ | 7674 packets / 7674 frames, 100 % parsed |
| Message type | ✅ | msgid=69 (MANUAL_CONTROL) / sysid=127 |
| Throttle range | ✅ | **−0.603 … +1.000** (centre +0.010) |
| Arming | ✅ | Throttle at bottom: 0 → 1 succeeded |
| Stick response | ✅ | 0.010 → 0.207 → −0.603 → +1.000, follows continuously |
| Battery voltage | ✅ | 4.15 V |
| Main loop rate | ✅ | 1000 Hz |
| Motor outputs (throttle NOT applied) | ✅ | All four channels 0 |
| Time base | ✅ | 3371 s uptime, wrap count 0 |
| **Motor position mapping** | ✅ | `mfr`→2, `mfl`→3, `mrr`→1, `mrl`→0 — all four spin, index matches position |
| **Roll mixing direction** | ✅ | Roll right: front-right 0.205 / rear-right 0.257 > front-left 0.088 / rear-left 0.134 |
| **Pitch mixing direction** | ✅ | Verified analytically (`Tools/mixer_sign_check.py`); `mot` readings unusable because `desaturate()` clips them |
| **`ca` static-bug fix** | ✅ | Scale factor rose from 1e-4 to the 1e-2 range, proving the accumulator starts from an empty set |
| **`help` menu fully printed** | ✅ | After the fix the console received **1720 bytes** — brand bar plus all 29 command lines (previously ~700 bytes, menu truncated) |
| **Boot banner fully printed** | ✅ | 2441 bytes — init log and the full motd arrived in one piece |
| **Hotspot name `YaoKun_` prefix** | ✅ | Aircraft #1 prints `当前热点名：YaoKun_F224`; holds on both the AP and the ESP-NOW path, the core default `ESP_xxxxxx` no longer appears. **Since v1.1 the suffix is read from the eFuse STA MAC** (older builds read the AP MAC → `YaoKun_6202`) |
| **Coexistence by default (v1.1)** | ✅ | Measured 2026-11-05: `WIFI_MODE=4`, hotspot `YaoKun_F224`, local IP `192.168.4.1`; the PC scanned `SSID YaoKun_F224 / BSSID 82:65:99:df:f2:24`; after `wifi espnow` and a reboot it stays in pure mode (migration runs only once) |
| **Release single-file firmware** | ✅ | 1,124,528 B / SHA-256 `7f8dafa3…4d4d2845`, flashing reported `Hash of data verified` |
| RC-loss failsafe | ⬜ pending | Pull the remote's power |
| Long-run wrap test | ⬜ pending | Needs > 71.6 minutes of continuous uptime |
| Six-face accelerometer calibration | ⚠️ not recommended | `ca` runs 48 s straight with no input wait, a human cannot keep up; the current bias is already zeroed and usable |

---

## ⚠️ "Phantom bugs" hit during testing (do not re-investigate)

| Symptom | What is really going on |
| --- | --- |
| `采样频率` shows 20 Hz / 1 Hz | That is **`loopRate` (the main-loop rate)**, not the IMU ODR. Normal during calibration and while idle — **do not touch `configureIMU()`** |
| `mot` readings are asymmetric, contain 0 and 1 | `desaturate()` **clips** the outputs, so the four channels are no longer comparable. Verify mixing direction offline with `Tools/mixer_sign_check.py` |
| Attitude completely wrong after running `ca` | The six faces were not actually flipped in time (48 s is too short). Recovery: `p IMU_ACC_SCALE_X/Y/Z 1` + `p IMU_ACC_BIAS_X/Y/Z 0` |

### ✅ Unified `YaoKun_` hotspot prefix (AP mode + ESP-NOW mode)

| Item | Value |
| --- | --- |
| Prefix | `YaoKun_` (capital Y and K) |
| Suffix | Last two bytes of the factory **STA MAC**, e.g. aircraft #1 = `YaoKun_F224` (corrected in v1.1) |
| Default passphrase | **`1-2-3-4-5-6-7-8`** (hyphens are separators only, do not type them; digit by digit: one, two, three, four, five, six, seven, eight). The source builds it from the ASCII table `{49,50,…,56,0}`; documents only ever show the hyphenated form, never a contiguous digit run (it gets rewritten in transit) |
| Finding the name | Only trust the boot log line `当前热点名：YaoKun_xxxx` |
| Renaming | `ap <ssid> <passphrase>` + `reboot` |

**Pitfall 1: ESP-NOW mode also brings up a hotspot.**
ESP-NOW can only run on an AP/STA interface, so the moment `WiFi.mode(WIFI_AP)` is called the core starts a
default hotspot named `ESP_` plus the last three MAC bytes. Older builds only overrode the `W_AP` branch, so in
ESP-NOW mode the aircraft broadcast the core default name and nobody could tell whose aircraft it was on site.
**Both paths must call `WiFi.softAP()`, and it must come after `mode(WIFI_AP)`** — otherwise the mode switch wipes it.

**Pitfall 2: the hotspot suffix must read the eFuse STA MAC (fixed in v1.1).**
Older builds used `WiFi.macAddress()`, which in AP / ESP-NOW context returns the **AP interface MAC**: aircraft #1
shows `本机 MAC：80:65:99:DF:F2:24` (STA) in `wifi`, while its AP MAC ends in `62:02`, so the hotspot became
`YaoKun_6202` and no longer matched the `wifi` output — you could not identify the aircraft by MAC on site.
It now reads the fuse directly via `esp_read_mac(ESP_MAC_WIFI_STA)` → always `YaoKun_F224`, so
**the last four hex digits of the MAC printed by `wifi` now reliably map to the hotspot name**.

**Pitfall 3: a placeholder hotspot name can be sitting in NVS.**
On aircraft #1 the NVS key `WIFI_AP_SSID` literally contained `"<ssid>"` (someone typed the documented placeholder
straight into `ap <ssid> <passphrase>` back then), so auto-naming was skipped and the hotspot was really called
`<ssid>`. v1.1 adds validation: a name containing `<` `>` or longer than 32 characters is treated as dirty data,
cleared automatically, and auto-naming is restored.

---

### 🔴 Real bug: `help` prints only the brand bar, the command menu disappears

**It is not "the menu was never written", it is truncated output.** Three things have to line up:

1. `motd` is one C string of **1718 bytes** (brand bar + 30 command lines), emitted in a single `Serial.print(buf)`;
2. The ESP32-S3 native USB path uses HWCDC, whose TX ring buffer is **only 256 bytes by default** (`setTxBufferSize(256)` in the core's `HWCDC.cpp`);
3. This project sets `setTxTimeoutMs(0)`, so `xRingbufferSend` inside `HWCDC::write()` uses a 0-tick timeout and whatever does not fit is **silently dropped on the spot**.

⇒ Only the first ~256 bytes make it into the buffer, and the cut happens to land exactly at the end of the brand bar,
so it looks like "the menu is missing" rather than "printing stopped mid-line".

**Fix** (`console.ino`, two parts working together):
- `setupConsole()` now calls `Serial.setTxBufferSize(4096)`, so the whole motd fits in one go; the USB interrupt
  drains it asynchronously and the main loop stays non-blocking;
- `handleConsole()` ends each command with `consoleFlushTail(150, ' ')` — writing one extra space re-arms the USB
  interrupt and carries the stranded tail out.

**Warnings from this fix**:
- ❌ Do not use `Serial.flush()`: with `setTxTimeoutMs(0)` it calls `flushTXBuffer(NULL, 0)`, which **discards the
  entire TX buffer** and marks the port as not connected;
- ❌ Do not write the drain test as `>= 4096`: the TX ring buffer is `BYTEBUF`, so the maximum free space is
  "capacity − 1" and that condition **is never true**.

### How to use `mot` to reset a polluted calibration

```bash
p IMU_ACC_SCALE_X 1
p IMU_ACC_SCALE_Y 1
p IMU_ACC_SCALE_Z 1
p IMU_ACC_BIAS_X 0
p IMU_ACC_BIAS_Y 0
p IMU_ACC_BIAS_Z 0
```

Afterwards `st` should show roll ≈ 0° / pitch ≈ 0° (measured 0.015° / −0.004°).

---

## 0. Preparation

| Item | Detail |
| --- | --- |
| Firmware | `Firmware/release/YaoKunS_耀坤S3无人机_Flix整理版_v1.1_耀坤智联_20261105_merged.bin` |
| Flash address | `0x0` (whole image in one go) |
| Serial port | **Native USB-CDC port** (`CDCOnBoot=cdc`), 115200, assert DTR when opening |
| Remote | YaoKun **Chinese** remote (`ESP32-DJI-Dev-Yaokun20260815`) |
| Other | **Power off the second aircraft** (same-frequency interference makes results unattributable) |

> ⚠️ If the board already holds calibration data in NVS, use **segmented flashing** to preserve `0x9000~0xE000`.

---

## 1. Correct remote operation (★ most commonly missed)

Confirmed by testing: the Chinese remote **does not enable control output by default**; until it is enabled it only
sends centre values. Full procedure:

1. **Power on** the remote.
2. Open the **"peer discovery"** page and wait for the aircraft to appear in the list (scanning crosses channels —
   *seeing* it does not mean *reaching* it).
3. **Tap** that aircraft → establish the link.
4. **Press the left (auxiliary) button** → enables control output (`enabled = !enabled`).
   > ⚠️ **There is no such switch in the UI — it is a physical button.** One press on, one press off.
   > ⚠️ **After the aircraft reboots you must reconnect and press it again** (the ESP-NOW session is gone).

### Channels must match

| Remote | Channel |
| --- | --- |
| Chinese (`ESP32-DJI-Dev-Yaokun20260815`) | **1** (Wi-Fi default, never set in code) |
| English (`ESP32-YKRC-Firmware`) | 6 (`esp_wifi_set_channel(6)` in `Control.hpp`) |

The firmware defaults to `espnowChannel = 1`. When swapping remotes, change the aircraft side with `ch <1-13>`:

```
> ch          → 信道：1        (channel: 1)
> ch 6        → ESP-NOW 信道已设为 6（遥控器必须一致），重启后生效
              (channel set to 6 — the remote must match; takes effect after reboot)
```

---

## 2. Boot banner (three fingerprint strings)

```
耀坤S3 飞控（Flix 整理版）启动中…
YaoKunS | 耀坤S3无人机 | Flix | 耀坤智联
固件版本：v1.1（耀坤整理版）
```
(line 1: "YaoKun S3 flight controller (Flix organized edition) starting…"
 line 2: product / airframe / firmware / vendor
 line 3: "Firmware version: v1.1 (YaoKun organized edition)")

- [ ] All three lines present (**the Chinese banner is the fastest way to tell this build from the English one**)
- [ ] `信道：1` (channel: 1)
- [ ] `原生 ESP-NOW 接收已就绪` (native ESP-NOW receive ready)

---

## 3. IMU detection (`imu`)

Aircraft #1 (LCSC board + MPU6500 over SPI):
```
型号：MPU-6500                       (model: MPU-6500)
芯片标识（WHO_AM_I）：0x70            (WHO_AM_I: 0x70)
总线：SPI　识别依据：SPI id　可用：1   (bus: SPI, detected by SPI id, available: 1)
引脚：SCK=38 MISO=21 MOSI=18 CS=17 INT=-1
状态：正常                            (status: OK)
```

Aircraft #2 (Taobao board + MPU6050 over I2C):
```
型号：MPU6050                        (model: MPU6050)
芯片标识（WHO_AM_I）：0x68            (WHO_AM_I: 0x68)
总线：I2C0　识别依据：I2C id　可用：1  (bus: I2C0, detected by I2C id, available: 1)
引脚：SDA=9 SCL=8 INT=-1
状态：正常                            (status: OK)
```

- [ ] Model matches the physical part
- [ ] Status: OK
- [ ] Chip temperature is a plausible value (≈40 °C; 0.0 means the bus is dead)

---

## 4. IMU calibration (`ca`) and orientation (`st`)

```
> ca      → 六步中文提示，8 秒/步，末行「校准完成！」
          (six Chinese prompts, 8 s per step, ends with "calibration complete!")
> st      → 机身右侧压低，横滚为正
          (lower the right side of the frame, roll should read positive)
```

- [ ] `ca` runs through to the end
- [ ] **Right side down → `横滚` (roll) positive** (the only orientation criterion; do not use raw acceleration)
- [ ] At rest and level, roll/pitch within ±2°

---

## 5. ESP-NOW link diagnostics (`wifi`)

```
> wifi
```
Key output:
```
接收包数：7674                          ← ① packets seen at the physical layer
接收字节数：16933                       ← ②
最近发包方 MAC：30:76:F5:EF:FE:08       ← ③ should be the remote's MAC
已解析 MAVLink 帧数：7674               ← ④
最近一帧消息 ID：69（MANUAL_CONTROL=69） ← ⑤
最近一帧来源 sysid：127                 ← ⑥ YaoKun remote = 127
MAVLink 链路在线：1
原生接收通道：1
接收队列积压：0 字节
```
(the labels mean, in order: packets received / bytes received / last sender MAC / parsed MAVLink frames /
last message ID / last frame source sysid / MAVLink link online / native receive channel / queue backlog)

**Decision table**:

| Symptom | Conclusion |
| --- | --- |
| Packet count = 0 | Physical layer not up → check channel, whether the remote is enabled, whether the peer is registered |
| Packets > 0, frames = 0 | Bytes arrive but no MAVLink frames form → the remote is in Raw protocol mode |
| Frames > 0, msgid ≠ 69 | The remote only sends HEARTBEAT (not enabled / not on the control page) |
| msgid = 69 but rc all zeros | Remote not enabled (press the left button), or target does not match `mavlinkSysId` |

---

## 6. Throttle range (★ must check before first flight)

`MANUAL_CONTROL` has a full scale of **±1000**; this project computes `controlThrottle = m.z / 1000.0f`.

| Stick position | Expected | Measured |
| --- | --- | --- |
| Bottom | ≈ **−1.0** | ✅ −0.603 observed (sampled during a fast pull-down) |
| Centre | ≈ **0.0** | ✅ +0.010 |
| Top | ≈ **+1.0** | ✅ **+1.000** |

**Conclusion: full ±1000 range, no 0~1000 mapping problem.**

❌ If you measure `0 / +0.5 / +1.0` (positive half only), **do not fly**.

---

## 7. Motors and arming

```
> mfr 0.1   → 右前  front-right    > mfl 0.1  → 左前  front-left
> mrr 0.1   → 右后  rear-right     > mrl 0.1  → 左后  rear-left
```

- [ ] All four motors spin, **position mapping correct** (the one that spins for `mfr` really is the front-right motor)
- [ ] Rotation direction matches the X-quad layout
- [ ] `mot` shows all four channels at 0 with the throttle down

Arming:
- [ ] Throttle to minimum → `arm` or arm from the remote → `rc` reports `解锁状态` (armed state) = 1
- [ ] Three short beeps + green LED blink
- [ ] Trying `arm` with the throttle off the bottom → prints "拒绝解锁：油门不在最低位" (arming refused: throttle not at minimum)

---

## 8. RC-loss failsafe

- [ ] Pull the remote's power
- [ ] Continuous tone + slow red blink within 1 second
- [ ] After 10 seconds the throttle goes to zero and the aircraft disarms itself

---

## 9. Long-run test (field proof of the R1 wrap fix)

The `time` command:
```
> time
运行时间：3371.271729 秒                     (uptime: 3371.271729 s)
主循环频率：1000 Hz                          (main-loop rate: 1000 Hz)
单步耗时：0.001000 秒                        (per-iteration time: 0.001000 s)
微秒计数器回绕次数：0（已吸收，不影响运行）    (microsecond-counter wraps: 0 — absorbed, harmless)
```

- [ ] Run continuously for **> 72 minutes** and confirm `t` increases monotonically and telemetry never stops
- [ ] Even if `微秒计数器回绕次数` (wrap count) is > 0, `运行时间` (uptime) **must never go backwards**

> Older builds died every 71.58 minutes (telemetry stopped completely while the console kept echoing).
> This item is the final proof that the fix works.

---

## 10. Troubleshooting

| Symptom | Check first |
| --- | --- |
| Remote does nothing at all | `接收包数` (packets received) in `wifi`; if 0 → check the channel (1 vs 6) |
| Remote says connected but the aircraft does not move | **Press the left button to enable** (`enabled` defaults to false) |
| Packets arrive but rc is all zeros | Is msgid 69 in `wifi`? If yes, press the left button to enable |
| Remote stops working after the aircraft reboots | Re-run "peer discovery → tap → press left button"; the session was cleared |
| No response in the lower half of the throttle | Range problem from section 6 — **do not fly** |
| IMU not detected | On aircraft #2 check SDA=9 / SCL=8; make sure the connector is fully seated |
| UI comes up in English | You flashed the English or the upstream build — look at the first boot line |
| Board stays in download mode after flashing | `esptool --after watchdog-reset run`, or unplug and replug USB |

---

## 11. After passing

All boxes ticked → push to the `yaokun-organized` branch of `github.com/YaoKunS/flix`.

Any ❌ → send me the raw console output for that item and we will locate it before changing anything.
