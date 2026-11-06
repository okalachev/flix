> Open-source flight controller Flix · YaoKun S3 hardware adaptation, v1.1

# YaoKun S3 Drone · Flix Organized Edition

*A teaching-grade flight controller kernel in under 2000 lines　·　native ESP-NOW receive rework　·　fully Chinese console*

- MCU　**ESP32-S3** (8 MB flash)
- Kernel　**Flix** ((C) Oleg Kalachev)
- Links　**ESP-NOW / MAVLink v2 / UDP**
- Control　**cascaded PID @ 500 Hz**
- UI　**Chinese** by default (English build available)
- Repo　**github.com/YaoKunS/flix**

🌐 [中文版](flix技术说明书.md) · English (this page)

## Contents

1. [What this project is](#1-what-this-project-is) —— What it is, and how it relates to upstream
2. [Hardware](#2-hardware) —— Pinout and peripherals
3. [Software architecture](#3-software-architecture) —— File responsibilities and the main loop
4. [ESP-NOW receive rework](#4-esp-now-receive-rework-the-core-change) —— The core change, with root-cause analysis
5. [Time base fix](#5-time-base-fix-the-716-minute-mystery) —— The 71.6-minute wraparound mystery
6. [IMU auto-detection](#6-imu-auto-detection) —— Two chips, two orientation brackets
7. [Control loops](#7-control-loops) —— Three cascaded loops and the mixer
8. [Safety mechanisms](#8-safety-mechanisms) —— Six gates and the link-loss failsafe
9. [LEDs and buzzer](#9-leds-and-buzzer) —— RGB signalling, non-blocking beeps
10. [The i18n system](#10-the-i18n-system) —— How i18n.h is designed
11. [Console commands](#11-console-commands) —— All 26 commands explained
12. [Building and flashing](#12-building-and-flashing) —— FQBN, single-file image, segmented writes
13. [Three-level verification](#13-three-level-verification) —— Static / bench / tethered
14. [Troubleshooting](#14-troubleshooting) —— Work backwards from the symptom
15. [Flight envelope](#15-flight-envelope) —— What it can and cannot do
16. [Differences from upstream](#16-differences-from-upstream) —— R1~R12 change summary
17. [MildlySpicyDragon 2.0 board](#17-mildlyspicydragon-20-board-support) —— second PCB & maintenance statement

## 1. What this project is

Flix is an open-source quadcopter flight controller firmware **designed for teaching and research**, written entirely in Arduino C/C++ and **under 2000 lines**. No RTOS, no closed-source libraries, no black boxes — sensor drivers, attitude estimation, PID control, motor mixing and MAVLink communication are all laid out in the source.

This project is the **YaoKun S3 hardware adaptation** of Flix: it runs upstream firmware on YaoKun's own quad frame, and it solves the key problem of "the aircraft does nothing after switching to our own ESP-NOW remote".

#### Upstream structure preserved

File layout, main-loop order, PID structure and the MAVLink implementation are all inherited. What changed is hardware parameters, the receive path and the UI language — not a rewritten kernel.

#### About the branches

`master` is byte-identical to upstream and can be fast-forwarded at any time. `yaokun-organized` is the branch that carries the adaptation.

> **💡 说明**
>
> **GPL obligations:** flix upstream **ships no separate LICENSE file**; copyright remains with the original author Oleg Kalachev. This repository keeps that attribution and repository URL, with the YaoKun information appended after it. **Do not declare any other licence.**

## 2. Hardware

### 2.1 Pin assignment (measured on the YaoKun S3)

| Function | GPIO | Notes |
|---|---|---|
| Motor FL (front left) | 16 | Four brushed motors, PWM at 20 kHz (above the audible range, so the motors do not scream) |
| Motor FR (front right) | 6 | Four brushed motors, PWM at 20 kHz (above the audible range, so the motors do not scream) |
| Motor RL (rear left) | 15 | Four brushed motors, PWM at 20 kHz (above the audible range, so the motors do not scream) |
| Motor RR (rear right) | 5 | Four brushed motors, PWM at 20 kHz (above the audible range, so the motors do not scream) |
| IMU SPI SCK / MISO / MOSI / CS | 38 / 21 / 18 / 17 | MPU-6500; the chip select is auto-probed (actual wiring may be 42 or 17) |
| IMU I2C SDA / SCL | 9 / 8 | MPU6050 module (8-pin socket) |
| Voltage sense | 2 | ADC2_CH1, divider ratio 1:2 |
| SBUS input | 1 | Disabled by default (ESP-NOW is used); change `RC_RX_PIN` only if you wire an SBUS receiver |
| Red / green / blue LED | 41 / 7 / 14 | low battery and link loss / connected and armed / initialising |
| Buzzer | 39 | Active buzzer, high-level trigger (driven through an S8050) |

### 2.2 Two IMU configurations

| Aircraft | IMU | Bus | WHO_AM_I | Orientation (about Z) |
|---|---|---|---|---|
| #1 | Domestic MPU6050 | I2C | 0x70 | **+π/2** |
| #2 | MPU-6500 | SPI | 0x70 | **−π/2** |

> **⚠️ 注意**
>
> **The only orientation criterion:** **press the right side of the airframe down**; the `横滚` (roll) value printed by `st` must be **positive**. Do not judge from raw accelerometer values in `imu` — they still carry mounting error and will mislead you.

## 3. Software architecture

### 3.1 File responsibilities

| File | Responsibility | YaoKun change |
|---|---|---|
| `flix.ino` | Main entry: setup / loop | Fingerprint banner, LED/buzzer init feedback |
| `config.h` | Board type and compile-time constants | YaoKun S3 board type |
| `i18n.h` | **Single source of UI strings** (169 entries, Chinese + English) | ★ new |
| `imu.ino` | IMU driver, detection, calibration | auto-detect + per-bus orientation + null guard |
| `estimate.ino` | Attitude estimation (complementary filter) | — |
| `control.ino` | Three cascaded loops + mixer | — |
| `motors.ino` | Motor PWM output | pins + 20 kHz |
| `rc.ino` | SBUS RC input | disabled by default |
| `wifi.ino` | Wi-Fi / UDP / ESP-NOW | ★ native receive callback |
| `mavlink.ino` | MAVLink v2 send/receive | unconditional ACK |
| `console.ino` | Command-line console | Chinese + the `ch` command |
| `led.ino` | LED and buzzer signalling | ★ new |
| `power.ino` | Voltage measurement | GPIO2 + divider 2 |
| `safety.ino` | Link-loss failsafe | — |
| `time.ino` | Time base | ★ 64-bit accumulator |
| `parameters.ino` | Parameter table and NVS persistence | 8 new parameters |
| `log.ino` | Log buffer and CSV dump | — |

### 3.2 Main loop order (do not reshuffle)

1. `readIMU()` —— Must be first: every control decision uses this frame's data ★
2. `step()` —— Updates time and dt; keep it next to the IMU read so dt matches data freshness ★
3. `readRC()` —— SBUS input (ESP-NOW input arrives via processMavlink)
4. `estimate()` —— Attitude estimation (gyro/acc + dt)
5. `control()` —— Three loops + failsafe
6. `sendMotors()` —— Output PWM as early as possible to cut control latency ★
7. `handleConsole() / processMavlink()` —— Comms go after control: no workload may delay motor output
8. `readVoltage / logData / syncParameters` —— Low-rate housekeeping
9. `updateLED()` —— Non-blocking state machine; last so it cannot disturb timing

> **⛔ 危险**
>
> **Never move communication ahead of `sendMotors()`.** Wi-Fi / ESP-NOW transfers occasionally take a few milliseconds; if that happens before the motor output, the PWM jitters and the PID shows high-frequency wobble or even oscillation. This order is inherited from upstream and it exists for a reason.

## 4. ESP-NOW receive rework (the core change)

### 4.1 Symptom

After flashing upstream 2026-09 onto the aircraft, the YaoKun ESP-NOW remote **did nothing, however it was connected**:

- the remote's UI showed **connected**
- the aircraft's console showed **no error at all**
- sticks pushed to the stops, the aircraft did not move
- while the 2026-08 build worked perfectly

### 4.2 Root cause (confirmed in the library source)

Upstream receives with:

```
int len = espnow.read(buf, size);
```

`ESPNOWSerial` is a **session-based** wrapper: it maintains a peer table internally and **only hands up data from registered peers**. The only way to register a peer is to receive a 4-byte **discovery packet** containing `"flix"` (which fires `onNewPeer`).

And upstream only sends discovery packets here:

```
// wifi.ino (upstream)
static Rate discovery(1);
if (discovery && espnow.isEncrypted()) {   // ← the gate
    espnowBroadcast.write((const uint8_t *)"flix", 4);
}
```

**Without encryption, no discovery packet is broadcast at all.** The YaoKun remote does not encrypt → the aircraft never advertises itself → the remote never learns the aircraft exists → no session → the aircraft's `espnow.read()` always returns 0 → **not one byte of stick data arrives**.

> **💡 说明**
>
> **Why does the remote say "connected" while nothing works?**
> The remote's criterion is **having received any ESP-NOW frame from the aircraft** (the telemetry broadcast), not "a bidirectional session exists". The two status displays therefore never agree — which is exactly what makes this bug so hard to find.

### 4.3 The fix

**步骤 1 · Register a native receive callback**
Bypass the session machinery and take every raw byte.

```
static QueueHandle_t recvQueue = nullptr;

static void IRAM_ATTR onReceive(const esp_now_recv_info_t *info,
                                const uint8_t *data, int len) {
    if (!espnowRecvInitialized) return;
    for (int i = 0; i < len; i++) {
        if (xQueueSendFromISR(recvQueue, &data[i], NULL) != pdTRUE) {
            lostPackets++;   // queue full: drop the byte and count it, never block the ISR
        }
    }
}
```

**步骤 2 · Read the receive path from the queue**
In ESP-NOW mode the queue is tried first; an empty queue returns 0.

```
int receiveWiFi(uint8_t *buf, int len) {
    if (wifiMode == W_ESPNOW || wifiMode == W_BOTH) {
        if (!espnowRecvInitialized || recvQueue == NULL) return 0;
        int i = 0;
        for (; i < len; i++)
            if (xQueueReceive(recvQueue, &buf[i], 0) != pdTRUE) break;
        if (i > 0 || !udpStarted) return i;   // UDP only once the queue is drained
    }
    ...
}
```

**步骤 3 · Broadcast discovery unconditionally**
The `isEncrypted()` gate is gone, so standard flix agents can discover this aircraft too.

### 4.4 Three details you must not get wrong

| Detail | Why |
|---|---|
| **Never call `esp_now_init()` again** | `ESPNOWSerial` has already initialised it. A second call returns `ESP_ERR_ESPNOW_INTERNAL` and wipes every existing peer — killing the transmit path too. Only call `esp_now_register_recv_cb()`. |
| **Queue elements are 1 byte, not whole frames** | The MAVLink parser is **streaming** (`mavlink_parse_char` is fed byte by byte). Storing bytes decouples "network reception" from "protocol parsing", so a partially received frame is never lost. |
| **When the queue is full you must drop, not wait** | The callback runs in the Wi-Fi task context. Blocking here stalls the whole Wi-Fi task → the aircraft stutters. Dropping the oldest bytes is the right trade. |

### 4.5 Channel: the easiest link to forget

ESP-NOW requires both ends on the **same channel**. On different channels the physical layer simply never meets: the symptom is "connected but dead", with no log output at all.

**The YaoKun organized edition defaults to `espnowChannel = 1`**, not 6. That came out of a bench-test correction:

> **⚠️ 注意**
>
> **⚠️ Bench test 2026-10-18: channel 6 → 1**
> The default was originally set to 6 after finding `esp_wifi_set_channel(6)` in the **English** remote firmware, but the **Chinese remote actually in use, `ESP32-DJI-Dev-Yaokun20260815`, never calls `esp_wifi_set_channel`** and stays on the Wi-Fi default channel **1**. The aircraft listened on 6 while the remote transmitted on 1 — a complete physical-layer mismatch.
> **What makes this failure so deceptive:** the remote's peer-discovery page **still lists the aircraft** (scanning crosses channels) and shows "connected" once selected, yet the aircraft's `rxPackets` stays at 0. **"Can see" ≠ "can talk".**

```
ch          → 信道：1
ch 6        → ESP-NOW 信道已设为 6（遥控器必须一致）
ch 11       → ESP-NOW 信道已设为 11（遥控器必须一致）
              以上重启后生效     (takes effect after reboot)
```

### 4.6 ESP-NOW peer registration: the second trap

The ESP-NOW receive callback **only fires for MACs registered in the peer table**; packets from unregistered devices are dropped in the driver and never reach the callback at all.

| How the remote transmits | Does the aircraft need a pre-registered peer? |
|---|---|
| **Broadcast** (`FF:FF:FF:FF:FF:FF`) | ❌ no |
| **Unicast** (specific target MAC) | ✅ **required**, otherwise everything is dropped |

> **⚠️ 注意**
>
> **⚠️ Bench test 2026-10-18: the Chinese remote unicasts after `connect(mac)`**
> The August remote broadcast, so the aircraft never needed to register a peer; with the Chinese remote the link became one-way and `rxPackets` stayed at 0.
> **Fix (in `wifi.ino`):**
> ① **pre-register the broadcast address** as a peer during init;
> ② record the source MAC in the `onReceive` callback and set a `peerLearnPending` flag;
> ③ actually call `esp_now_add_peer()` from `learnPeerIfNeeded()` in the main loop.
> **🔴 Why not call add_peer directly in the callback:** `esp_now_add_peer()` takes the Wi-Fi lock, and the callback already runs in the Wi-Fi task context — calling it there **self-deadlocks** and freezes the aircraft.

### 4.7 Remote side: the enabled switch (third trap)

> **⚠️ 注意**
>
> **⚠️ The Chinese remote defaults to `enabled = false`**
> While disabled the remote **still sends** `MANUAL_CONTROL`, but all four axes sit at centre (≈0.01). The symptom is: "packets arrive, frames parse, msgid=69, sysid=127, yet `rc` never moves".
> **To enable it: press the remote's "left button" (secondary button)** (in code, `onSecondaryButtonClick` → `enabled(!enabled)`). **There is no such switch in the UI; it is a physical button only.**
> **⚠️ After every aircraft reboot you must repeat: peer discovery → select the aircraft → press the left button to enable.**

### 4.8 How to confirm the fix

Read these numbers from the `wifi` command, in this order:

| Observation | Conclusion |
|---|---|
| `接收包数` (packets) = 0 | physical layer down → check the **channel** (§4.5) |
| packets rise but `已解析 MAVLink 帧数` (frames) = 0 | protocol mismatch → check baud rate / frame header / MAVLink v2 |
| frames rise but `最近一帧消息 ID` ≠ 69 | the remote is sending something else, not `MANUAL_CONTROL` |
| msgid = 69 but all four `rc` axes sit at 0.01 | **remote not enabled** → press the left button (§4.7) |
| everything above and the axes follow the sticks | ✅ the link is healthy |

Measured while passing: **21782 packets / 21782 frames (100 % parsed)**, msgid=69, sysid=127, throttle range **−0.603 ~ +1.000** (centre +0.010, confirming the full ±1000 range).

### 4.9 Hotspot naming rule (`YaoKun_` prefix)

The YaoKun edition unifies the aircraft hotspot name behind a brand prefix, and hooks the generation logic into **both the AP mode and ESP-NOW mode** paths.

| Item | Rule |
|---|---|
| Prefix | **`YaoKun_`** (note the capital `Y` and `K`) |
| Suffix | the last two bytes of the chip's **eFuse factory STA MAC** (4 uppercase hex digits, different per board; this rule starts with v1.1 — the old AP-MAC behaviour is obsolete) |
| Default passphrase | **`1-2-3-4-5-6-7-8`**<br>the hyphens are separators and **are not typed**; the real passphrase is 8 characters, i.e. eight digits, position *n* holds digit *n*<br>All documentation writes only this **hyphenated** form — contiguous digit runs get rewritten by some transfer/render paths (hit repeatedly on this project; it is how the v1.0 docs published a wrong passphrase). The source builds it from the ASCII byte table `{49,50,51,52,53,54,55,56,0}` |
| Actual value | the boot log line `当前热点名：YaoKun_xxxx` — **that line is authoritative** |
| Custom name | `ap <ssid> <passphrase>` + `reboot`; once a custom value is stored in NVS the prefix is no longer auto-generated |

> **💡 说明**
>
> **Factory passphrase, digit by digit (type it key by key; readable in any language)**

| Position | 1st | 2nd | 3rd | 4th | 5th | 6th | 7th | 8th |
|---|---|---|---|---|---|---|---|---|
| Character | `1` | `2` | `3` | `4` | `5` | `6` | `7` | `8` |

> Enter 8 characters with no spaces or symbols in between. The full string is deliberately never printed here: contiguous digit runs get rewritten as they pass through rendering paths. That is not fussiness, it is a bug we already paid for.

> **💡 说明**
>
> **Why the ESP-NOW path had to change too**
> ESP-NOW can only run on an AP/STA Wi-Fi interface, so `WiFi.mode(WIFI_AP)` is required in `wifiMode == W_ESPNOW` as well. But as soon as the core switches to AP mode it **spins up a default hotspot** with SSID `ESP_` plus the last three MAC bytes and no passphrase. The old build only overrode the `W_AP` branch, so "in ESP-NOW mode the aircraft still broadcasts a hotspot, but it is called `ESP_xxxxxx`" — impossible to identify in the field, and passers-by could join it.
> Fix: call `WiFi.softAP(apSsid, apPass)` on both paths. The order matters — `softAP()` must come **after** `mode(WIFI_AP)`, otherwise the mode switch resets it.

> **💡 说明**
>
> **✅ The suffix equals the last two bytes of the MAC printed by `wifi` (corrected in v1.1)**
> Aircraft #1: `本机 MAC：80:65:99:DF:F2:24` → hotspot **`YaoKun_F224`**; aircraft #2 → `YaoKun_DE38`.
> The old build used `WiFi.macAddress()`, which in AP / ESP-NOW context returns the **AP interface MAC** (last two bytes `62:02` → hotspot name `YaoKun_6202`), which did not match the `wifi` output.
> It now uses `esp_read_mac(ESP_MAC_WIFI_STA)` to read the chip's eFuse directly, which returns the same factory value in every mode. **There is no hard-coded suffix in the code**, so each board differs automatically.

> **💡 说明**
>
> **⚠️ NVS may hold a placeholder hotspot name (found in testing)**
> Aircraft #1 had the literal `<ssid>` stored in `WIFI_AP_SSID` (someone once typed the documentation placeholder into `ap <ssid> <passphrase>`), so auto-naming was skipped and the hotspot really was called `<ssid>`.
> v1.1 adds validation: empty name / contains `<` `>` / longer than 32 characters → treated as dirty, removed from NVS and auto-naming restored to `YaoKun_xxxx`.

> **💡 说明**
>
> **Why the suffix is the MAC rather than "aircraft 1 / aircraft 2"**
> Numbering would require flashing a different firmware per aircraft, which directly contradicts "one firmware for every board". The MAC is unique at manufacture, so every board differs automatically. A fixed name (the old `Yaokun_Drone`) is equally bad the other way round — two aircraft powered at once **collide on the same SSID**; the phone sees only one, connects to whichever it likes, and in a small field you cannot tell which is which.

### 4.10 AP and ESP-NOW coexistence (the v1.1 default)

The target user is a newcomer following the docs: assemble it and you should get "the remote flies it and the phone shows telemetry", without running a console command first. From v1.1 the factory default is `WIFI_MODE=4` (`W_BOTH`), with both links active at once.

**Why coexistence was impossible before**: the physical layer already coexisted — ESP-NOW must run on an AP/STA interface, so the moment `WiFi.mode(WIFI_AP)` is called the hotspot is broadcasting. What actually blocked the ground station was two pieces of software:

- `udp.begin()` was only called in the `W_AP` / `W_STA` branches → in ESP-NOW mode nothing was listening on UDP, so a ground station that joined the hotspot got no reply at all;
- `sendWiFi()` did `if (espnow) { …; return; }` → telemetry only ever left over ESP-NOW.

**The change**: a new `udpStarted` flag (set only by `setupWiFi()`), and the TX/RX paths use it to decide "can we use UDP" instead of testing the mode enum everywhere. Three key points:

- **Set the channel exactly once**: `WiFi.softAP(ssid, pass, espnowChannel)`, instead of "softAP then setChannel" — a second channel switch kicks already-connected clients, and the remote may drop packets during the switch. AP and ESP-NOW share one radio, so **a channel mismatch means immediate physical-layer loss with no error message**.
- **TX: ESP-NOW first, UDP only when someone is connected**: `if (!udpStarted || WiFi.softAPgetStationNum() == 0) return;` — with no client attached, all airtime goes to the remote.
- **RX: only read UDP once the queue is empty**: the two byte streams **never share a buffer**. The MAVLink parser is streaming, and interleaving two streams makes it resync constantly — losing frames at best, decoding a wrong payload length at worst.

> **💡 说明**
>
> **🔴 The biggest trap when upgrading: NVS overrides the compiled default**
> `WIFI_MODE` is a persisted parameter, and `setupParameters()` overwrites the default with the NVS value. Old boards store `3` (ESP-NOW only), so **changing only the default changes nothing**. Hence the one-shot migration `migrateWifiModeToCoexist()`: an NVS marker `COEX_VER` upgrades 3 to 4 on first boot; a later explicit `wifi espnow` is not switched back. Out-of-range values are rescued to `W_BOTH` as well — an illegal value makes `setupWiFi()` match no branch, which shows up as "no hotspot after boot and the remote dead too".

| Command | Hotspot | ESP-NOW remote | UDP ground station | Use |
|---|---|---|---|---|
| `wifi both` (default) | ✅ | ✅ | ✅ 14550 | Normal flying: remote and QGC online together |
| `wifi espnow` | ✅ (no UDP) | ✅ | ❌ | Bisecting airtime interference |
| `wifi ap` | ✅ | ❌ | ✅ 14550 | Parameter tuning only |
| `wifi sta` | ❌ | ❌ | ✅ | Join a home router |
| `wifi off` | ❌ | ❌ | ❌ | Radio off |

> **💡 说明**
>
> **⚠️ Do not enable QGC's virtual joystick**
> `handleMavlink()` does not validate the source of `MANUAL_CONTROL`. In coexistence mode QGC and the physical remote can both send it, which shows up as "the sticks do nothing" or "the sticks jump around".

## 5. Time base fix: the 71.6-minute mystery

### 5.1 Symptom

After roughly **71.6 minutes** of uptime all telemetry stops, the console still echoes commands, and `reboot` restores normal operation. A different build or a power cycle "fixes" it, so it is extremely hard to reproduce and very easy to blame on Wi-Fi.

### 5.2 Root cause

```
// upstream
t = micros() / 1000000.0f;
```

On ESP32 Arduino, `micros()` returns **uint32**, maxing out at 4294967295 µs ≈ **4294.97 s ≈ 71.58 min**, after which it **wraps to 0**. So `t` jumps from 4294.97 to 0 — **time runs backwards**.

Every rate gate in the project has this shape:

```
Rate telemetrySlow(2);
if (telemetrySlow) { ... }   // internally tests t - lastTime >= 1/rate
```

`t - lastTime` becomes negative → the condition never succeeds → **every periodic task stalls at once**: telemetry, logging, parameter sync, LED updates.

> **⚠️ 注意**
>
> **But `handleConsole()` is unaffected** — it is purely event driven (`Serial.available()`). That "the console still works" observation is precisely what sends people down the "Wi-Fi problem" path, and it is why this bug stayed hidden for so long.

### 5.3 The fix

```
static uint64_t timeUs = 0;
static uint32_t lastMicros = 0;

const uint32_t nowMicros = micros();
const uint32_t deltaUs = nowMicros - lastMicros;  // unsigned: correct delta even across a wrap
lastMicros = nowMicros;
timeUs += deltaUs;
t = timeUs / 1000000.0f;
```

> **✅ 正确做法**
>
> **The key trick:** `deltaUs` uses **unsigned subtraction**. At the wrap instant (`nowMicros` small, `lastMicros` large) the unsigned result wraps around automatically and still yields the correct positive delta. That is why a 64-bit accumulator is immune to wraparound.

Two probes were added: `tRewinds` (wrap count) and `tRewindAt` (last wrap timestamp), both visible in the `time` command.

### 5.4 Why not `esp_timer_get_time()`

It returns int64 and indeed never wraps, but it adds a new dependency and **the symbol does not exist in the Gazebo simulation environment**. The accumulator needs nothing and also runs in simulation.

## 6. IMU auto-detection

### 6.1 Why auto-detection

`FlixPeriph`'s `IMU::create(model, bus)` returns `nullptr` for unsupported model/bus combinations (MPU6050 can only run on I2C). Upstream **does not check for null** before calling `imu->begin()`, and `readIMU()` calls `imu->read()` directly — **null-pointer dereference**.

Symptom: flash aircraft #1's firmware (I2C 6050) onto aircraft #2 (SPI 6500) and it crashes at boot, the console printing half a line.

### 6.2 Detection flow

1. `probeI2C(0x08~0x77)` —— read WHO_AM_I (register 0x75) over Wire
2. `MPU6050 family → I2C, orientation +π/2` —— the 8-pin socket physically rejects a 6500 module
3. `probeSPI(candidate CS: imuCsPin / 42 / 17)` —— CS wiring differs between PCB batches, so try each
4. `MPU6500 family → SPI, orientation −π/2` —— remember which CS actually worked
5. `imuOK = false` —— arming blocked + red LED solid + long beep ★

### 6.3 Null guard (what actually prevents the crash)

```
if (imu == nullptr || imu->status() != 0) {
    imuOK = false;
    print(TXT_IMU_UNUSABLE);   // IMU unusable — arming disabled
    return;
}

void readIMU() {
    if (!imuOK || imu == nullptr) return;   // ← first line
    ...
}
```

> **✅ 正确做法**
>
> **Design trade-off:** when the IMU is unavailable the aircraft is **not** allowed to die — it is "marked unusable and arming is blocked". It can still connect, show logs and accept parameter changes, so in the field you can tell whether the socket is loose or the chip is dead instead of facing a board that "does nothing when plugged in".

### 6.4 Orientation lock

Parameter `IMU_ROT_LOCK`: once set to 1, auto-detection no longer overwrites `imuRotation`. **Set it to 1 after running `ca`** — detection probes on every boot, and if one probe fails or WHO_AM_I lands in an ambiguous range, an already-calibrated orientation can be overwritten with the wrong value.

```
p IMU_ROT_LOCK 1
```

### 6.5 Six-face accelerometer calibration (`ca`): a real C++ trap

The maths of six-face calibration is simple: six orientations cover ±1 g on each axis, so

```
accScale = (accMax − accMin) / 2 / G      // ideal ≈ 1.0
accBias  = (accMax + accMin) / 2          // ideal ≈ 0
```

> **⚠️ 注意**
>
> **🐛 Bug found on the bench 2026-10-18: a `static` local is initialised only once**
> The original code declared the extreme-value accumulators **inside** `calibrateAccelOnce()`:
> `static Vector accMax(-INFINITY, ...);`
> `static Vector accMin(INFINITY, ...);`
> **A C++ `static` local is initialised only on the <u>first</u> call**; afterwards it keeps the previous values. So the second time `ca` ran, `accMax`/`accMin` still held leftovers from the previous run (or from power-up), and `accScale` was completely wrong — **measured 0.000257 instead of ≈ 1.0**, with accelerometer readings becoming `−0.86 / 0.19 / 9.19` and the attitude badly off.
> **Fix:** promote them to file-scope globals `accCalMax`/`accCalMin` and **reset them explicitly** to ±INFINITY at the top of `calibrateAccel()`.
> **Verification:** with the same (mis)operation, the scale went from `0.000257` (1e-4 range) to `0.0222` (1e-2 range) — a 100× improvement in magnitude, proving the accumulator now started clean instead of inheriting data.

> **⚠️ 注意**
>
> **⚠️ Usability flaw in `ca`: it runs 48 seconds straight, and human hands cannot keep up**
> The sequence is: `print "step 1/6"` → `pause(8)` → sample → `print "step 2/6"` → `pause(8)` → sample …… **it never waits for input** and finishes in about 48 seconds.
> If the operator fails to get the airframe into the required orientation inside each 8-second window, **all six samples land in the same orientation** and `accScale` collapses to the 1e-2 range. **This is a design flaw, not an operator error.**
> **Proper fix:** split `ca` into six single-step commands `ca1`~`ca6`, each sampling one face and waiting for confirmation (not implemented in the current version).

> **✅ 正确做法**
>
> **Recovering from a bad calibration** (no reflashing required):
> `p IMU_ACC_SCALE_X 1`　`p IMU_ACC_SCALE_Y 1`　`p IMU_ACC_SCALE_Z 1`
> `p IMU_ACC_BIAS_X 0`　`p IMU_ACC_BIAS_Y 0`　`p IMU_ACC_BIAS_Z 0`
> After these six commands the attitude returns to zero immediately (measured roll 0.015° / pitch −0.004°). The MPU-6500's factory accuracy is good enough to use directly — **you lose no precision**.

> **💡 说明**
>
> **💡 Do not be misled by "采样频率" (sampling frequency):** the value printed by `imu` is **`loopRate` (the main-loop frequency)**, <u>not</u> the IMU output data rate (ODR).

- while calibrating, `pause()` runs a slow `delay(50)` loop → shows **20 Hz**
- when idle the main loop blocks in `waitForData()` → shows **1 Hz**
- during normal flight → **1000 Hz**

> All three values are normal; **do not go and change `configureIMU()`**.

## 7. Control loops

### 7.1 Three cascaded loops

1. `Attitude loop (angle → rate target)` —— P control on roll/pitch angle error
2. `Rate loop (PID)` —— with integral clamping (windup) and derivative low-pass
3. `Quad mixer → four throttle outputs` —— X layout: throttle plus three-axis torque
4. `PWM 20 kHz → motors` —— with idle throttle and saturation protection ★

### 7.2 Verifying mixer direction (without flying)

There is an unavoidable obstacle: **once any motor output is clipped by `desaturate()`** (clamped to 0 or 1), the four readings are no longer comparable, and comparisons like "is the front higher than the rear" **become meaningless**.

> **⚠️ 注意**
>
> **⚠️ Trap hit on the bench 2026-10-18:**
> reading `mot` gave
> `FR 1.000 (clipped high) / FL 0.193 / RR 0.077 / RL −0.393 (clipped low)`
> That data shows **neither pitch nor roll** — only that the operator tilted the airframe diagonally. **Verifying mixer direction from `mot` readings does not work.**

**The right approach: offline mathematical verification.** The tool `Firmware/tools/mixer_sign_check.py` reproduces the firmware's sign chain in Python exactly and feeds it known attitudes:

```
control.ino:82   upActual  = rotate(up, attitude)
control.ino:85   error     = rotationVectorBetween(upTarget, upActual)
vector.h:113     direction = cross(a, b)          ← a=target, b=actual
vector.h:128     return direction * angle
control.ino:87-88 ratesTarget.x/y = PID(error.x/y)   ← positive gains, sign preserved
control.ino:101-102 torqueTarget.x/y = PID(rateError)
control.ino:119-122 mixer (X layout):
    FL = T + tx − ty + tz
    FR = T − tx − ty − tz
    RL = T + tx + ty − tz
    RR = T − tx + ty + tz
```

| Attitude | Correct expectation | Reasoning |
|---|---|---|
| nose pushed down | **front thrust > rear** | push the front up → nose-up torque (seesaw: to lift the dropping end, push under that end) |
| nose pitched up | **front thrust < rear** | push the rear up → nose-down torque |
| right side pushed down | **right thrust > left** | push the right up → roll-left torque |
| left side pushed down | **left thrust > right** | push the left up → roll-right torque |

> **✅ 正确做法**
>
> **✅ Result (2026-10-18): pitch and roll mixer directions are both correct.**
> **Cross-check:** the script's "right side down → more thrust on the right" matches the bench measurement exactly (tilted right: FR 0.205 / RR 0.257 > FL 0.088 / RL 0.134), which means the reasoning and the firmware behaviour agree and the conclusion is trustworthy.

> **⛔ 危险**
>
> **🔴 This criterion was once written backwards — recorded here to prevent a regression:** on a quad-X all four motors sit **in the airframe plane** and torque comes from thrust differences. Pushing the nose down means **adding thrust at the front** (lifting the nose end), <u>not</u> adding thrust at the rear. Getting this backwards inverts the whole verification conclusion.

### 7.3 Four flight modes

| Mode | Enum | Behaviour |
|---|---|---|
| RAW | 0 | Fully manual; sticks map straight to the motors (debugging) |
| ACRO | 1 | Rate control; the sticks command angular rates |
| **STAB** | 2 | **Self-levelling (default)**: the sticks command angles, releasing them re-levels |
| AUTO | 3 | Accepts MAVLink attitude/torque targets |

### 7.4 MANUAL_CONTROL range semantics (important)

The protocol defines `x/y/z/r` as int16 with a full range of **±1000**. This project:

```
controlThrottle = m.z / 1000.0f;   // z: -1000 ~ +1000 → -1 ~ +1
controlPitch    = m.x / 1000.0f;
controlRoll     = m.y / 1000.0f;
controlYaw      = m.r / 1000.0f;
```

> **⛔ 危险**
>
> **Must check on the bench:** if the remote actually sends **0 ~ 1000** (treating centre as 0), the stick centre is already 0 throttle and thrust only appears in the upper half — which feels like **"only half the travel, and it jumps at mid-stick"**.
> On the first bench session confirm: **stick at minimum → z ≈ -1000, centre → z ≈ 0, maximum → z ≈ +1000**. Use the `rc` command to read the normalised throttle.

## 8. Safety mechanisms

### 8.1 Link-loss failsafe (`safety.ino`)

| Condition | Behaviour |
|---|---|
| remote / ground station all silent for **1 s** | throttle ramps down |
| silent for **10 s** | throttle to zero + automatic disarm |
| tilt beyond `SF_DISARM_TILT` | treated as a tumble, disarm |

> **⚠️ 注意**
>
> **The "all four sources silent" trap:** this firmware detects link loss by "no control input received". If you simply put the remote down and no ground station is sending heartbeats, **it starts ramping the throttle down after 1 second**. That is designed behaviour, not a bug. During bench testing, keep exercising a stick or attach a ground station.

### 8.2 Pre-arm checks

- the IMU must be usable (`imuOK`)
- the throttle must be at minimum (normalised < 0.05), otherwise arming is refused with `拒绝解锁：油门不在最低位`
- the tilt must be within the threshold

### 8.3 Unconditional command ACK (R2)

`handleMavlinkCommand()` only **decides the result code and performs side effects**; it never sends an ACK itself. The caller sends it unconditionally after the call returns:

```
int result = handleMavlinkCommand(&m);
mavlink_message_t ack;
mavlink_msg_command_ack_pack(..., m.command, result, ...);
sendMessage(&ack);   // ← sent for accepted / denied / unsupported alike
```

This makes it **structurally impossible** to forget the ACK when adding a command, so QGC never hangs on "executing".

## 9. LEDs and buzzer

### 9.1 LED priority

| Priority | Condition | Behaviour |
|---|---|---|
| 1 (highest) | voltage < 3.5 V | red LED flashing at 100 ms + continuous fast beeping |
| 2 | connected and armed | green LED flashing at 200 ms |
| 3 | connected, not armed | green LED solid |
| 4 | not connected | red LED flashing at 500 ms |
| 5 | initialising | blue LED solid |

### 9.2 Buzzer patterns

| Code | Sound | Trigger | Meaning |
|---|---|---|---|
| 'S' | single short | power-up | initialisation started |
| 'D' | double short | init done / remote connected | ready |
| 'T' | triple short | arming succeeded | armed |
| 'L' | long | RC lost / IMU unusable | alarm |
| 'C' | continuous fast | low battery | loops until voltage recovers |

The semantics match the YaoKun in-house controller **YaoKunDrone** — "short / double / triple / long / continuous" feel identical, so users do not have to relearn anything when moving between the two systems.

### 9.3 Why the buzzer must be non-blocking

> **⛔ 危险**
>
> **Calling `delay()` in the main loop to play a tone breaks the 1 kHz loop immediately**, the attitude estimate misses steps, and ⇒ **the aircraft flips the instant you arm**. This is a pitfall we actually hit.

So the design is "state machine driven by the main loop":

- `buzzerPlay(char)` only sets parameters and returns immediately
- `buzzerUpdate()` advances the timing from the main loop (the first thing `updateLED()` does)
- only the two tones in `setup()` use `runBuzzerFor(ms)` to block briefly (arming is impossible then, so blocking is harmless)

### 9.4 The connection-criterion trap

The semantically equivalent upstream code decides "ground station online" from `mavlinkTime`, but `mavlinkTime` is refreshed by **any** MAVLink frame — ground-station heartbeats, parameter requests, even the aircraft's own frames counted via loopback. The result is **"connected even with no remote attached"** — the LEDs lie.

This edition uses `controlTime`:

```
const bool connected = valid(controlTime) && (t - controlTime < 1.0f);
```

`controlTime` is **only updated when MANUAL_CONTROL (a stick frame) arrives**, which is the true meaning of "remote online". If SBUS is enabled, `readRC()` updates it as well, so both paths are correct.

## 10. The i18n system

### 10.1 Design

All UI text lives in `Firmware/flix/i18n.h` behind `TXT_*` macros, with `#ifdef FLIX_LANG_EN` selecting a Chinese or English block. **Chinese is the default.**

### 10.2 Why not write Chinese straight into the `.ino` files

#### Keeps the English build path alive

A GPL repository has to serve overseas users. With Chinese hard-coded in 30-plus print() calls, switching languages means editing line by line — easy to miss one, and the diff becomes enormous.

#### Scriptable verification

Once strings are centralised, "is any English left in the UI" becomes automatable: just confirm every print() goes through TXT_*.

### 10.3 Building the English version

```
// Option 1: add one line at the very top of flix.ino (before the first #include)
#define FLIX_LANG_EN

// Option 2: pass the macro at build time, no file edits
arduino-cli compile --build-property "compiler.cpp.extra_flags=-DFLIX_LANG_EN" ...
```

### 10.4 Conventions

- macros expand to **string literals**, so write `print(TXT_XXX)`, never `print("%s", TXT_XXX)`
- parameterised messages are "prefix macro + format string" so each language reads naturally: `print(TXT_CH_SET, ch)`
- **this file must be saved as UTF-8 without BOM**. If Notepad saves it as GBK, the Chinese becomes mojibake bytes sent straight to the serial port

### 10.5 Verification result

> **✅ 正确做法**
>
> **169** macros in each of the Chinese and English blocks; the script confirms no one-sided entries ("English only" and "Chinese only" are both empty) and no references to undefined macros. Two macros are defined but unreferenced (`TXT_NO` / `TXT_YES`), which the checker reports explicitly.

## 11. Console commands

Connect over serial (115200, **native USB port**) and type `help`. The Chinese build (the default) prints:

```
可用命令：
help / motd - 显示本帮助
p - 列出全部参数
p <前缀> - 显示以该前缀开头的参数
p <参数名> <数值> - 修改参数（自动存盘）
preset - 恢复出厂参数（会重启）
time - 显示时间与主循环频率
imu - 显示惯性测量单元数据
ca - 校准加速度计（六面校准）
st - 显示姿态估计结果
arm / disarm - 解锁 / 上锁
raw/stab/acro/auto - 切换飞行模式
rc - 显示遥控（ESP-NOW）数据
cr - 校准遥控行程
pw - 显示电源与电压
wifi - 显示无线连接信息
wifi ap/sta/espnow/both/off - 设置无线模式
ap <热点名> <密码> - 设置飞机热点
sta <路由名> <密码> - 连接家里的路由器
espnow <遥控器MAC> [<密钥>] - 设置 ESP-NOW 对端
ch [<信道>] - 查看或设置 ESP-NOW 信道
mot - 显示四路电机输出
log [dump] - 打印日志表头 [以及数据]
mfr/mfl/mrr/mrl [<油门>] - 单独测试电机（务必拆桨！）
sys - 显示系统信息
reset - 复位姿态估计
reboot - 重启飞机
```

English gloss (the English build prints this directly):

```
help / motd              show this help
p                        list all parameters
p <prefix>               list parameters starting with the prefix
p <name> <value>         set a parameter (saved automatically)
preset                   restore factory parameters (reboots)
time                     show time base and main-loop rate
imu                      show IMU data
ca                       calibrate the accelerometer (six-face)
st                       show the attitude estimate
arm / disarm             arm / disarm
raw/stab/acro/auto       select flight mode
rc                       show RC (ESP-NOW) data
cr                       calibrate the RC travel
pw                       show power and voltage
wifi                     show wireless link info
wifi ap/sta/espnow/both/off   set the wireless mode
ap <ssid> <pass>         set the aircraft hotspot
sta <ssid> <pass>        join a home router
espnow <remote MAC> [<key>]   set the ESP-NOW peer
ch [<channel>]           show or set the ESP-NOW channel
mot                      show the four motor outputs
log [dump]               print the log header [and data]
mfr/mfl/mrr/mrl [<thr>]  test one motor (REMOVE PROPELLERS!)
sys                      show system info
reset                    reset the attitude estimate
reboot                   reboot the aircraft
```

### 11.1 New parameters (R10)

| Parameter | Default | Description |
|---|---|---|
| `IMU_AUTO_DETECT` | 1 | auto-detect the IMU at boot (0 = use the manual IMU_MODEL value) |
| `IMU_ROT_LOCK` | 0 | freeze the orientation so detection stops overwriting it |
| `LED_R_PIN` / `LED_G_PIN` / `LED_B_PIN` | 41 / 7 / 14 | RGB LED pins |
| `LED_LOW_VOLT` | 3.5 | low-battery warning threshold (V) |
| `BUZ_PIN` | 39 | buzzer pin |
| `BUZ_ENABLE` | 1 | buzzer master switch (0 = silent, for indoor debugging at night) |

## 12. Building and flashing

### 12.1 Build

```
cd Firmware
arduino-cli compile \
  --fqbn "esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=8M,PartitionScheme=default_8MB,PSRAM=disabled" \
  --libraries "G:\Arduino\libraries" \
  --export-binaries flix
```

> **⛔ 危险**
>
> **`--export-binaries` is mandatory.** Without it the `build/` directory is not refreshed, and the packaging script happily assembles a bin that "looks fine but contains the wrong firmware" — a pitfall we have actually hit.

### 12.2 Packaging the single-file image

```
python Firmware/tools/pack_flix_release.py --version 1.1
```

Output lands in `Firmware/release/`:

- `*_merged.bin` — the single-file firmware, flash at `0x0`
- `*_指纹与烧录说明.md` / `.txt` — generated automatically; hashes and segment layout are extracted from the bin at that moment
- `*_Fingerprint_and_Flashing_Guide_EN.md` / `.txt` — the English edition of the same document, built from the same data

### 12.3 Merge layout

| Address | Content | Notes |
|---|---|---|
| `0x0` | bootloader.bin | second-stage bootloader |
| `0x8000` | partitions.bin | partition table |
| `0x9000` | *nvs (gap, filled with 0xFF)* | **a whole-chip write erases this** |
| `0xE000` | boot_app0.bin | initial OTA data; without it the chip will not boot |
| `0x10000` | app.bin | the application |

> **⚠️ 注意**
>
> **Flashing the single file from 0x0 = erasing NVS.** The Wi-Fi hotspot name/passphrase, the auto-detected IMU result and the RC endpoint calibration are all lost.
> On a new board that is fine; **on a machine you have already calibrated, flash in segments** and skip `0x9000`. The offsets come from `flix/build/<fqbn>/flash_args`.

### 12.4 Stuck in download mode after flashing

If the log shows `waiting for download`, GPIO0 is being held low. Connect once with `--after no-reset` to confirm the image is readable, then run:

```
esptool --after watchdog-reset run
```

Or simply replug USB / press RST.

## 13. Three-level verification

### 13.1 Level 1 · static

| Check | Method | Pass criterion |
|---|---|---|
| compiles | `arduino-cli compile ... --export-binaries` | exit code 0, no warnings |
| zh/en pairing | script compares macro counts in both i18n.h blocks | 169 : 169, no one-sided entries |
| English build works | rebuild with `-DFLIX_LANG_EN` | exit code 0 |
| no English leftovers | scan `print("` calls not using `TXT_` | only pure format strings and CSV headers remain |
| footprint | compiler output | Flash < 90 %, RAM < 60 % |

### 13.2 Level 2 · bench (**propellers must be removed**)

| Step | Action | Expected result |
|---|---|---|
| 1 | power up, read the first screen | Chinese banner + fingerprint string + `固件版本：v1.1` |
| 2 | `imu` | model / bus / WHO_AM_I / orientation correct, `状态：正常` |
| 3 | `ca` | six Chinese prompts, 8 s each, last line `校准完成！` |
| 4 | `st` while tilting | right side down → `横滚` positive (**the only orientation criterion**) |
| 5 | power the remote, `rc` | raw channels follow the sticks, the timestamp keeps refreshing |
| 6 | `ch` | `ESP-NOW 信道：1` (must match the remote) |
| 7 | `wifi` | `原生接收` = 1, dropped bytes barely move |
| 8 | stick at min / centre / max | normalised throttle **-1 / 0 / +1** |
| 9 | `mfr 0.1` motor by motor | direction and position correct (propellers off!) |
| 10 | `arm` / `disarm` | triple beep on arm / LED switches to green flashing |
| 11 | cut power to the remote | long beep + slow red flashing after 1 s |

### 13.3 Level 3 · tethered low hover

**步骤 1 · Prepare**
Fit propeller guards, tether the aircraft, use open space.

**步骤 2 · Take off**
`arm`, then raise the throttle slowly until it lifts 10~20 cm.

**步骤 3 · Verify the correction direction**
Tap roll/pitch and confirm the **correction goes the right way** (tilt right → corrects right).

**步骤 4 · Release the sticks**
Confirm the attitude re-levels (but **it keeps drifting horizontally** — expected, there is no position sensor).

**步骤 5 · Watch the LEDs**
green flashing = armed, red flashing = link lost, fast red = low battery.

## 14. Troubleshooting

| Symptom | Most likely cause | How to confirm |
|---|---|---|
| remote says connected, aircraft does not move | ESP-NOW channel mismatch / native receive not registered | `ch` for the channel; `wifi` for whether `原生接收` is 1 |
| remote says connected, aircraft does not move | remote sends 0~1000 instead of ±1000 | `rc` for the normalised throttle range |
| all telemetry stops 71.6 min after power-up | R1 not in effect (old firmware flashed) | `time` for `回绕次数`; verify the SHA-256 |
| red LED solid + long beep at boot | IMU detection failed | `imu` for `识别依据` and the pins |
| crashes / reboot loop at boot | wrong board firmware (IMU model mismatch) | check the first screen's `识别为 …` against the actual chip |
| attitude inverted (tilting right shows left) | wrong IMU orientation bracket | `st` while tilting: right side down must give **positive** roll |
| arming rejected | throttle not at minimum | console prints `拒绝解锁：油门不在最低位` |
| phone disconnects, UI freezes | router and aircraft AP on the same channel | separate them with `ch`, change the remote to match |
| packaged firmware is wrong | compiled without `--export-binaries` | compare the modification time of the bin in the build directory |
| **`help` shows only the banner, no command list** | the HWCDC transmit buffer is only 256 B, so the 1.7 KB motd is truncated on write (see 14.1) | check whether the output ends with `reboot - 重启飞机`; fixed in this version (TX buffer raised to 4 KB + tail flush) |
| no log when only the UART header is connected | the console runs over native USB (CDCOnBoot=cdc) | use the native USB port |

### 14.1 The `help` menu printed only half of itself (fixed)

> **💡 说明**
>
> **Symptom**: after flashing, typing `help` shows only the brand banner (YaoKunS3 flight controller / repo / Taobao), and the "available commands:" heading plus the 29 command lines **never appear**; typing `help` again shows only the banner. Short-output commands like `time`, `rc` and `pw` all behave normally.

**Three things had to line up before it broke**; individually none of them looks like a problem:

| # | Factor | Fact |
|---|---|---|
| ① | `motd` is one long C string | banner + 30 command lines = **1718 bytes**, written in a single `Serial.print(buf)` after `vsnprintf` |
| ② | The ESP32-S3 native USB path is HWCDC | its transmit ring buffer is **only 256 bytes by default** (`setTxBufferSize(256)` in the core's `HWCDC.cpp`) |
| ③ | This project sets `setTxTimeoutMs(0)` | writes never block — `HWCDC::write()` calls `xRingbufferSend` with a 0-tick timeout, so whatever does not fit **breaks out silently and is dropped** |

⇒ 1718 bytes pushed into a 256-byte buffer: only the first ~256 bytes get in; before the USB interrupt can move them to the host, the remaining 1400+ bytes are dropped. The drop point falls exactly after the banner, so it looks like "the menu was never written" rather than "output stopped mid-way" — which is why the first diagnosis was "the menu is not in the code at all".

**Two changes were needed together**:

1. **Enlarge the buffer**: `Serial.setTxBufferSize(4096)` in `setupConsole()`. The whole motd fits into the ring buffer in one go and the USB interrupt moves it to the host asynchronously. The main loop stays fully non-blocking (`setTxTimeoutMs(0)` is unchanged), so flight timing is unaffected. Cost: 4 KB of heap.
2. **Flush the tail**: enlarging alone is not enough — the "last chunk" lingers in the buffer until the next write carries it out (the HWCDC interrupt is only re-armed at the end of `write()`). So the command handler in `handleConsole()` calls `consoleFlushTail(150, ' ')` at the end: it appends one space (invisible, does not split the line) to re-arm the interrupt, then waits for the buffer to drain.

> **💡 说明**
>
> **⚠️ Two traps**
> ① **Do not use `Serial.flush()`**: with `setTxTimeoutMs(0)` in this project, `HWCDC::flush()` enters with `tries = 0` → goes straight to `flushTXBuffer(NULL, 0)`, which **discards** the whole transmit buffer and marks the port as disconnected.
> ② **Do not test for drain with `>= 4096`**: the TX ring buffer is a `BYTEBUF`, so the maximum available space is "capacity − 1" (one byte distinguishes full from empty); that condition is **never true** and every call would burn the full timeout. Instead treat "available space has stopped growing over two consecutive polls" as drained, which is independent of capacity.

## 15. Flight envelope

> **⛔ 危险**
>
> This firmware is **attitude stabilization plus a manual throttle**, with no position or velocity sensor at all (**no GPS / barometer / rangefinder / optical flow**). `control.ino` has three loops only (attitude → rate → mixer).

| What you might want | What actually happens |
|---|---|
| Release the sticks and hover | **No.** The attitude levels out, but the aircraft keeps drifting at its current horizontal velocity |
| Release the throttle and hold altitude | **No.** It descends immediately (there is no altitude loop) |
| Automatic landing on low battery | No landing; only a low-battery warning (fast red flashing + beeping) |
| Return to home on link loss | No RTH; only 1 s of throttle ramp-down and a zero-and-disarm at 10 s |
| Wind resistance | The attitude loop rejects disturbances, but the wind still pushes the aircraft off position — an unavoidable consequence of having no position sensor |

> **✅ 正确做法**
>
> **The right mental model:** treat it as "an aircraft that will level itself for you", not "an aircraft that will stay put by itself". Your hand should be on the stick the whole time.

## 16. Differences from upstream

| ID | Files | Change | Type |
|---|---|---|---|
| **R1** | `time.ino` | `t` produced by a 64-bit microsecond accumulator | 🔴 critical bug fix |
| **R2** | `mavlink.ino` | `COMMAND_LONG` always ACKed; rejection reason printed | 🟠 protocol robustness |
| **R4** | `wifi.ino` | native ESP-NOW callback + byte queue; discovery broadcast unconditionally; **default channel 1** (the Chinese remote never calls `esp_wifi_set_channel`, so it stays on the Wi-Fi default of 1); **broadcast peer pre-registration + remote MAC auto-learning** (`learnPeerIfNeeded()`) | 🔴 **core fix** |
| **R5** | `motors.ino` | motor pins 15/5/6/16, PWM 20 kHz | 🔧 hardware adaptation |
| **R6** | `power.ino` | voltage on GPIO 2, divider ratio 2 | 🔧 hardware adaptation |
| **R7** | `imu.ino` | auto-detect + per-bus orientation + `imuOK` guard | 🔴 crash fix |
| **R8** | `led.ino` | RGB LED + non-blocking buzzer state machine | ✨ new |
| **R9** | `i18n.h` | 169 strings, Chinese default, English build available | ✨ new |
| **R10** | `parameters.ino` | 8 new parameters | ✨ new |
| **R11** | `wifi.ino` / `i18n.h` | ESP-NOW + hotspot coexistence by default (`W_BOTH`); SSID suffix from eFuse STA MAC | ✨ new |
| **R12** | `wifi.ino` / `i18n.h` | three-layer passphrase validation (sanitise NVS / reject bad writes / byte-table factory value) | 🐞 bug fix |
| — | `flix.ino` | boot fingerprint banner; LED/buzzer init feedback | ✨ new |
| — | `console.ino` | Chinese motd + brand banner + `ch` command | ✨ new |
| — | `rc.ino` | SBUS disabled by default; Chinese calibration prompts | 🔧 hardware adaptation |
| — | `tools/pack_flix_release.py` | single-file firmware + automatic fingerprint documents (Chinese and English) | 🛠 release tooling |

> **💡 说明**
>
> The item-by-item root-cause analysis and verification methods are in [`Docs/YaoKunS3_Adaptation_Notes_EN.md`](YaoKunS3_Adaptation_Notes_EN.md) (Chinese original: `Docs/耀坤S3适配说明.md`). This page is the illustrated version of that document.

## 17. MildlySpicyDragon 2.0 board support

Since v1.1, a single source tree supports two PCBs through a **compile-time board macro** (`board.h`, one board = one set of `PIN_*` macros): **YaoKun S3** (default) and the **MildlySpicyDragon 2.0** “ESP32 mini drone”(ESP32 DevKit V1, 30-pin). Building the MildlySpicyDragon variant requires an explicit `-DFLIX_BOARD=2` — the motor pins carry no self-identifying signal, and a wrong guess makes motors spin up out of order at power-on, so there is **no runtime board auto-detection**.

### 17.1 MildlySpicyDragon 2.0 pin assignment

| Peripheral | Pins | Notes |
|---|---|---|
| Motors M1 FR / M2 RR / M3 RL / M4 FL | GPIO 33 / 27 / 26 / 32 | LEDC PWM, 38 kHz |
| IMU (SPI) | SCK 18 / MOSI 23 / MISO 19 / CS 5 | MPU6500 / MPU6050 on socket headers, never both at once |
| I2C bus | SDA 21 / SCL 22 | first bus (I2C0) |
| Battery voltage ADC | GPIO 35 | divider ratio 2.0 (same as YaoKun S3) |
| Buzzer | GPIO 25 | active type, level-driven |
| LED indicator | GPIO 2 (on-board, single) | RGB semantics unchanged — `led_map.h` ORs the three channels into one LED; state is expressed by **blink frequency** |
| SBUS receiver | GPIO 4 | Serial 2 |
| Optical flow (reserved) | MOS 13 / CLK 14 / MIS 12 / CS 15 | PMW3901, Espressif ESP-Drone scheme |
| Laser (reserved) | SCL1 16 / SDA1 17 | VL53L1X, second bus (I2C1) |

> **💡 说明**
>
> **Attribution & maintenance statement**: the MildlySpicyDragon 2.0 “ESP32 mini drone”hardware was designed and open-sourced by **MildlySpicyDragon (malagis)** ([OSHWHub](https://oshwhub.com/malagis/esp32-mini-plane)). **The firmware for that board — the board support and maintenance of the Flix YaoKun organized edition — is maintained and adapted by YaoKunSmart (耀坤智联)**. Kernel copyright (C) Oleg Kalachev (GPL obligation). The YaoKun S3 board and the MildlySpicyDragon board are two independent hardware products: the MildlySpicyDragon boot banner shows the MildlySpicyDragon branding, while the YaoKun S3 banner contains no MildlySpicyDragon information.

**YaoKunSmart / 耀坤智联**  
Bilibili: https://space.bilibili.com/3707011699050767 · OSHWHub: https://oshwhub.com/lindahe  
GitHub: https://github.com/YaoKunS · Taobao: https://shop35432590.taobao.com/

Kernel copyright (C) Oleg Kalachev · https://github.com/okalachev/flix  
Second-stage development by YaoKunSmart (directory organisation / Chinese UI / hardware adaptation) · version v1.1
