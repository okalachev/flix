# YaoKun S3 Adaptation Notes (`yaokun-organized` branch)

> This document records every code change the **YaoKun organized edition** of Flix makes relative to upstream `okalachev/flix`, together with a **root-cause analysis** and a **verification method** for each one.
> When you finish reading it you should be able to answer three questions: what changed, why it had to change, and how to prove it was fixed correctly.

- Upstream: <https://github.com/okalachev/flix> (`(C) Oleg Kalachev`)
- This branch: `yaokun-organized` in <https://github.com/YaoKunS/flix>
- Version: **v1.1**
- Hardware: YaoKun S3 drone (ESP32-S3, 8 MB flash)
- Reference implementation used for comparison: `flix20260819yaokun`, flight-proven in 2026-08

> 🌐 **中文版**: [`耀坤S3适配说明.md`](耀坤S3适配说明.md) — section-for-section equivalent, for Chinese readers.

---

## 1. The conclusion in one paragraph

Upstream receives ESP-NOW data through a **session-based** wrapper, but the YaoKun remote **never opens a session**,
so the remote shows "connected" while the aircraft does nothing at all. This edition takes over reception with a
**native ESP-NOW callback plus a byte queue**, and at the same time fixes the time base, IMU detection, LED/buzzer
signalling and UI language — the result is a board that can fly out of the box.

---

## 2. Change summary

| ID | Files | Change | Type |
| ------- | ---------------------------- | ------------------------------------------------------------ | -------------- |
| **R1**  | `time.ino`                   | `t` is now produced by a 64-bit microsecond accumulator | 🔴 critical bug fix |
| **R2**  | `mavlink.ino`                | `COMMAND_LONG` always answered with an ACK; rejection reason printed | 🟠 protocol robustness |
| **R4**  | `wifi.ino`                   | ESP-NOW reception moved to a native callback + FreeRTOS byte queue; discovery packets broadcast unconditionally; default channel 1 | 🔴 **core fix** |
| **R5**  | `motors.ino`                 | Motor pins `15/5/6/16`, PWM 20 kHz | 🔧 hardware adaptation |
| **R6**  | `power.ino`                  | Voltage sense on GPIO 2, divider ratio 2 | 🔧 hardware adaptation |
| **R7**  | `imu.ino`                    | IMU auto-detected at boot (I2C 6050 / SPI 6500) + orientation chosen per bus + `imuOK` guard | 🔴 crash fix + hardware adaptation |
| **R8**  | `led.ino`                    | RGB LED + buzzer non-blocking state machine; connection test now uses `controlTime` | ✨ new feature |
| **R9**  | `i18n.h` (new)               | All UI strings centralised, Chinese by default, `FLIX_LANG_EN` switches to English | ✨ new feature |
| **R10** | `parameters.ino`             | New parameters `IMU_AUTO_DETECT` / `IMU_ROT_LOCK` / `LED_*` / `BUZ_*` | ✨ new feature |
| **R11** | `wifi.ino` / `i18n.h`        | **ESP-NOW + hotspot coexistence by default** (`wifi both`); SSID suffix read from eFuse STA MAC | ✨ new feature |
| **R12** | `wifi.ino` / `i18n.h`        | Three-layer passphrase validation: sanitise dirty NVS on read + reject illegal writes + build the factory passphrase from a byte table | 🐞 fixes "cannot join the hotspot" |
| —       | `flix.ino`                   | Boot fingerprint banner; reordered init sequence (LED/buzzer feedback) | ✨ new feature |
| —       | `console.ino`                | Chinese motd + brand banner + `ch` command | ✨ new feature |
| —       | `rc.ino`                     | SBUS disabled by default (ESP-NOW instead); Chinese calibration prompts | 🔧 hardware adaptation |
| —       | `tools/pack_flix_release.py` | Single-file firmware packaging + automatic fingerprint document generation | 🛠 release tooling |

---

## 3. Root-cause analysis, item by item

### R1 · 32-bit time base wraparound (🔴 the most insidious one)

**Symptom**: after roughly **71.6 minutes** of uptime all telemetry stops, while the serial console still echoes
commands; `reboot` restores normal operation.
Flashing a different build or power-cycling "fixes" it, which makes it extremely hard to reproduce and very easy to
misdiagnose as "Wi-Fi dropped".

**Root cause**:

```cpp
// upstream
t = micros() / 1000000.0f;
```

On ESP32 Arduino, `micros()` returns **uint32**, so it maxes out at 4294967295 µs ≈ **4294.97 s ≈ 71.58 min** and
then **wraps to 0**. `t` therefore jumps from 4294.97 to 0 — **time moves backwards**.

Every rate gate in the project has this shape:

```cpp
Rate telemetrySlow(2);
if (telemetrySlow) { ... }        // internally: t - lastTime >= 1/rate
```

`t - lastTime` becomes negative → the condition never succeeds → **every periodic task stops at once**: telemetry,
logging, parameter sync, LED updates. But `handleConsole()` is purely event driven (`Serial.available()`), so it keeps
working — and that is exactly what pushes people towards a "Wi-Fi problem" theory.

**Fix**: accumulate the **delta** of each `micros()` call into a 64-bit counter, which is inherently immune to
wraparound:

```cpp
static uint64_t timeUs = 0;
static uint32_t lastMicros = 0;
const uint32_t nowMicros = micros();
const uint32_t deltaUs = nowMicros - lastMicros;  // unsigned arithmetic: correct even across a wrap
lastMicros = nowMicros;
timeUs += deltaUs;
t = timeUs / 1000000.0f;
```

Because `deltaUs` uses **unsigned** subtraction, the wrap instant (`nowMicros` small, `lastMicros` large) still yields
the correct positive delta — that is the whole trick. Two probes were added as well:

- `tRewinds`: number of wraps, visible in the `time` command
- `tRewindAt`: timestamp of the most recent wrap

**Why not simply use `esp_timer_get_time()`**: it returns int64 and indeed never wraps, but it introduces a new
dependency and **the symbol does not exist in the Gazebo simulation environment**. The accumulator needs no
dependencies and runs in simulation too.

**Verification**: in the `time` command, look at `微秒计数器回绕次数` (microsecond-counter wrap count). If it is > 0
while telemetry keeps running, the fix is in effect.

---

### R4 · ESP-NOW receives nothing from the remote (🔴 the core issue)
**Symptom**: after moving to upstream 2026-09, the YaoKun ESP-NOW remote "does nothing" no matter how you connect it.
The remote's UI shows *connected* (it receives the aircraft's `flix` discovery broadcast, or the telemetry broadcast),
and the aircraft's serial console shows **no error at all** — it simply does not move. The 2026-08 build worked fine.

**Root cause** (confirmed by reading the `ESP32_NOW_Serial` library source):
upstream receives with

```cpp
int len = espnow.read(buf, size);
```

`ESPNOWSerial` is **session based**: it keeps a peer table internally and **only hands up data from registered
peers**. The only way a peer gets registered is a 4-byte **discovery packet** containing `"flix"` (which fires the
`onNewPeer` callback).

And upstream only sends discovery packets here:

```cpp
// wifi.ino (upstream)
static Rate discovery(1);
if (discovery && espnow.isEncrypted()) {     // ← the gate
    espnowBroadcast.write((const uint8_t *)"flix", 4);
}
```

**Without encryption, no discovery packet is ever broadcast.** The YaoKun remote does not encrypt → the aircraft never
advertises itself → the remote never learns the aircraft exists → no session is created → `espnow.read()` on the
aircraft always returns 0 → **not a single byte of stick data arrives**.

That also explains "the remote shows connected": the remote's criterion is *having received any ESP-NOW frame from the
aircraft* (the telemetry broadcast), not *a bidirectional session being established*. The two sides' status displays
therefore disagree completely.

**Fix** (following the approach proven in 2026-08):

1. Register an **additional native** receive callback that pushes every byte into a FreeRTOS queue:

```cpp
static QueueHandle_t recvQueue = nullptr;

static void IRAM_ATTR onReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    if (!espnowRecvInitialized) return;
    for (int i = 0; i < len; i++) {
        if (xQueueSendFromISR(recvQueue, &data[i], NULL) != pdTRUE) {
            lostPackets++;   // queue full: drop the byte and count it, never block the ISR
        }
    }
}
```

1. `receiveWiFi()` drains the queue **first** in ESP-NOW mode:

```cpp
if (wifiMode == W_ESPNOW || wifiMode == W_BOTH) {   // W_BOTH = coexistence mode from R11
    if (!espnowRecvInitialized || recvQueue == NULL) return 0;
    int i = 0;
    for (; i < len; i++) if (xQueueReceive(recvQueue, &buf[i], 0) != pdTRUE) break;
    if (i > 0 || !udpStarted) return i;              // UDP only once the queue is empty
}
```

1. Discovery packets are now broadcast **unconditionally** (the `isEncrypted()` gate is gone), so standard flix
   agents can discover this aircraft as well.

**Three details you must not get wrong**:

| Detail | Why |
| ------------------------- | ------------------------------------------------------------------------------------------------------------------ |
| **Do not call `esp_now_init()` again** | `ESPNOWSerial` already initialised it. A second call returns `ESP_ERR_ESPNOW_INTERNAL` and wipes every existing peer — which also kills the transmit path. Only call `esp_now_register_recv_cb()`. |
| **Queue elements are 1 byte, not whole frames** | The MAVLink parser is **streaming** (`mavlink_parse_char` is fed byte by byte). Storing bytes decouples "network reception" from "protocol parsing", so a partially received frame is never lost. |
| **When the queue is full you must drop, not wait** | The callback runs in the Wi-Fi task context. Blocking here stalls the whole Wi-Fi task → the aircraft stutters. Dropping the oldest bytes is the correct choice. |

**Verification**:

- The boot log shows `ESP-NOW 原生接收已启用` (native ESP-NOW reception enabled)
- `wifi` shows `原生接收` = 1 and `队列字节` fluctuating
- Moving the sticks while watching `rc` makes `原始通道` change
- `接收丢弃字节数` (mapped to `lostPackets`) should barely move; if it climbs, the host cannot keep up

**Channel**: ESP-NOW requires both ends on the **same channel**. **This edition defaults to `espnowChannel = 1`**
(corrected by bench testing on 2026-10-18, see the next section).
A new `ch` command was added so you do not have to edit code and rebuild:

```
ch          → 信道：1
ch 6        → ESP-NOW 信道已设为 6（遥控器必须一致），重启后生效
```

---

### R4 addendum · 2026-10-18 bench testing: three chained traps

> This section only surfaced through **testing on real hardware**; pure code review cannot find it. The three traps are
> chained, and failing to clear any one of them produces the same symptom — *"the remote shows connected, the aircraft
> does nothing, no errors anywhere"* — from three different root causes.

#### Trap 1 · Wrong default channel (6 → 1)

| Item | Detail |
| --- | --- |
| **Symptom** | The aircraft's `wifi` shows `接收包数：0` while the remote shows "connected" |
| **Root cause** | The default was set to 6 based on `esp_wifi_set_channel(6)` in the **English** remote firmware (`Control.hpp`). But the **Chinese** remote actually in use, `ESP32-DJI-Dev-Yaokun20260815`, **never calls** `esp_wifi_set_channel`, so it stays on the Wi-Fi default channel **1**. Aircraft listens on 6, remote transmits on 1 — the physical layer never meets. |
| **What makes it deceptive** | The remote's "peer discovery" page **still lists the aircraft**, because scanning crosses channels. *"Can see it" does not mean "can talk to it".* |
| **Fix** | `espnowChannel = 1` (matching the flight-proven `Flix-YaoKun/flix enhanced build`) |
| **When swapping remotes** | Align both sides. The English remote uses 6, the Chinese one uses 1; change the aircraft side with `ch <1-13>`. |

#### Trap 2 · The aircraft never registers a peer, so all unicast packets are dropped

| Item | Detail |
| --- | --- |
| **Symptom** | After aligning channels, `接收包数` is still 0 |
| **Root cause** | A hard rule of ESP-NOW: **only packets from MACs registered in the peer table trigger the receive callback**; unregistered sources are dropped in the driver and never reach the callback. After `connect(mac)`, the Chinese remote unicasts all stick data to the aircraft's MAC, while the aircraft only called `esp_now_register_recv_cb()` and **never `esp_now_add_peer()`** — a one-way link. |
| **Why the August build was fine** | It broadcast (`FF:FF:FF:FF:FF:FF`), and broadcast packets need no pre-registered peer. |
| **Fix** | ① Pre-register the broadcast address as a peer (so discovery-phase packets get through); ② record the source MAC in `onReceive` and let `learnPeerIfNeeded()` in the main loop call `esp_now_add_peer()` (**you cannot add from inside the callback — it would deadlock on the Wi-Fi lock**). |
| **Verification** | The first time a new peer appears the console prints `已登记新的 ESP-NOW 对端：30:76:F5:EF:FE:08` |

#### Trap 3 · The remote's `enabled` flag defaults to false and sends centre values only

| Item | Detail |
| --- | --- |
| **Symptom** | `接收包数` grows normally, `msgid=69` is correct, yet the `rc` stick values sit at 0.01 and never move |
| **Root cause** | The Chinese remote initialises `ControlService::_enabled{false}` — **disabled by default**. While disabled, `if (_enabled) { sample sticks }` in `pollImpl()` does not run, but `MANUAL_CONTROL` **is still sent**, with all four axes at centre (≈0). Hence "packets arrive, frames parse, no data". |
| **What to do** | **Press the remote's "left button" (secondary button)** → `onSecondaryButtonClick()` does `enabled(!enabled)`. **There is no such switch in the UI; it is a physical button only.** |
| **How to tell** | Once enabled, moving the sticks makes the four `rc` values follow immediately |

#### Diagnostic fields added for this (`wifi` command)

Debugging this class of problem cannot rely on "is there an error message"; you must be able to answer "which layer did
the packets reach". `wifi` now prints:

```
接收包数：737          ← ① ESP-NOW packets seen at the physical layer (0 = channel / not sending / peer not registered)
接收字节数：16933       ← ② total bytes
最近发包方 MAC：30:76:F5:EF:FE:08   ← ③ who is sending (should be the remote's MAC)
已解析 MAVLink 帧数：737 ← ④ how many frames could be assembled (much lower than packets = protocol mismatch)
最近一帧消息 ID：69（MANUAL_CONTROL=69） ← ⑤ is it a stick packet? (2=HEARTBEAT means no sticks are being sent)
最近一帧来源 sysid：127  ← ⑥ remote system id (YaoKun = 127; check this when target filtering is suspected)
MAVLink 链路在线：1
```

**How to read it**:

| Observation | Conclusion |
| --- | --- |
| packets = 0 | physical layer down → check the channel (trap 1), whether the remote is enabled and sending, whether the peer is registered (trap 2) |
| packets > 0, frames = 0 | bytes arrive but no MAVLink frame can be assembled → the remote is in raw protocol mode |
| frames > 0, msgid ≠ 69 | the remote only sends HEARTBEAT, no sticks (not enabled / not on the control page) |
| msgid = 69, `rc` all zero | the remote is not enabled (trap 3), or the target does not match `mavlinkSysId` |

**Final bench result (aircraft #1, 2026-10-18)**:

| Item | Result |
| --- | --- |
| packets / frames | 7674 / 7674 (100 % parsed) |
| message id / source sysid | 69 (MANUAL_CONTROL) / 127 ✓ |
| throttle range | **-0.603 ~ +1.000** (centre +0.010) ✓ **full ±1000 range, not 0~1000** |
| arming | `解锁状态` flips 0 → 1 with throttle at minimum ✓ |
| stick response | 0.010 → 0.207 → -0.603 → +1.000, follows continuously ✓ |

---

### R4 addendum · Where the channel and peer code lives

| Function | Location |
| --- | --- |
| Default channel | `wifi.ino` · `int espnowChannel = 1;` |
| Peer auto-learning | `wifi.ino` · `learnPeerIfNeeded()`, called from the main loop in `flix.ino` |
| Receive statistics | `wifi.ino` · `rxPackets / rxBytes / rxLastMac` |
| Frame statistics | `mavlink.ino` · `rxMsgCount / rxLastMsgId / rxLastSrcSys` |
| Bench tools | `Firmware/tools/bench_read.py`, `bench_cmd.py`, `bench_i2c_scan.py` |

---

### R2 · Missing command ACK hangs the ground station

**Symptom**: in QGC, pressing "arm" while the aircraft refuses makes the UI spin until it times out; switching flight
modes with an illegal parameter does the same.

**Root cause**: upstream returns `MAV_RESULT_DENIED` directly from the rejection branch. The current version happens to
send the ACK from the `COMMAND_LONG` branch using the returned value, but earlier versions did drop it. The robust
approach is to make omission **structurally impossible**.

**Fix**: `handleMavlinkCommand()` only **decides the result code and performs side effects**; it never sends an ACK
itself. The caller sends the ACK unconditionally after the call returns:

```cpp
int result = handleMavlinkCommand(&m);
mavlink_message_t ack;
mavlink_msg_command_ack_pack(..., m.command, result, ...);
sendMessage(&ack);   // ← sent for accepted / denied / unsupported alike
```

Rejections are also printed to the console in Chinese (`拒绝解锁：油门不在最低位` / `设置模式被拒绝：模式号无效`), so
you know why something was refused without a ground station attached.

**Verification**: in QGC, raise the throttle and press arm → the rejection comes back immediately (instead of
spinning), and the console prints the reason.

---

### R7 · Wrong IMU model → null-pointer crash

**Symptom**: flash aircraft #1's firmware (MPU6050, I2C) onto aircraft #2 (MPU-6500, SPI) and it crashes on boot.

**Root cause**: `FlixPeriph`'s `IMU::create(model, bus)` returns `nullptr` for unsupported model/bus combinations
(MPU6050 can only run on I2C). Upstream `setupIMU()` **does not check for null** before calling `imu->begin()`, and
`readIMU()` calls `imu->read(...)` directly → **null-pointer dereference**.

**Fix**:

1. **Auto-detect at boot**: read the WHO_AM_I register (0x75) directly to identify the actual chip.

```cpp
// probeI2C(): scan I2C addresses with Wire and read 0x75
// probeSPI():  pull each candidate CS low and read 0x75
// WHO_AM_I values: 0x70/0x68/0x72 → MPU6050 family; 0x70/0x71/0x73/0x74/0x98/0x75 → MPU6500 family
```

1. **Orientation chosen per bus** (single criterion: **press the right side down → `st.roll` must be positive**):

| Bus | IMU | `imuRotation` | Note |
| --- | -------- | -------------- | ---------------------------- |
| I2C | MPU6050  | `(0, 0, +π/2)` | Domestic 6050 modules are mounted 180° opposite to the 6500 |
| SPI | MPU-6500 | `(0, 0, −π/2)` | |

> ⚠️ **Only the sign of `st.roll` counts** — do not use the raw accelerometer values from `imu`, which still carry
> mounting error.
> ⚠️ **Always re-run `ca` when you change the IMU** (six-face accelerometer calibration); bias and scale differ per chip.

1. **`imuOK` guard** (this is what actually prevents the crash):

```cpp
if (imu == nullptr || imu->status() != 0) {
    imuOK = false;
    print(TXT_IMU_UNUSABLE);   // IMU unusable — arming disabled (check the socket / wiring)
    return;
}
```

```cpp
void readIMU() {
    if (!imuOK || imu == nullptr) return;   // ← first line
    ...
}
```

When the IMU is unavailable, `setup()` holds the **red LED on and beeps for 700 ms**, so in the field it is obvious this
is not a "setting" problem.

**Verification**: the first console screen should show

```
正在自动识别惯性测量单元…
  识别为 MPU-6500 系（SPI）SCK=38 MISO=21 MOSI=18 CS=17
  芯片标识（WHO_AM_I）= 0x70
  安装朝向补偿：绕 Z 轴 -1.5708 弧度  [按总线分档]
惯性测量单元就绪：MPU6500，总线 SPI
```

#### R7 addendum · Automatic compatibility with both PCB revisions + full-range I2C scan (bench test 2026-10-18)

**Requirement**: YaoKun has two PCB revisions — V1 (the old board, already flying) and V2 (rewired according to
`耀坤S3无人机引脚定义.xlsx`). **One firmware, no rebuild** must run on both.

| Signal | V1 old board (measured) | V2 new board (pin table) | Handling |
| --- | --- | --- | --- |
| MPU6500 SCK / MISO / MOSI | 38 / 21 / 18 | 38 / 21 / 18 | identical, fixed |
| **MPU6500 CS** | **GPIO17** | **GPIO42** | candidate list `{imuCsPin, 42, 17}` probed in order |
| MPU6500 INT | GPIO17 (shared with CS) | GPIO40 | **not used** (timer sampling, not interrupts) |
| MPU6050 SDA / SCL | GPIO9 / GPIO8 | GPIO9 / GPIO8 | identical, fixed |

> ⚠️ The V2 table says `MPU6500_CS = GPIO42`, `MPU6500_INT = GPIO40`; on the V1 board CS measures as 17.
> The candidate list tries both, and whichever returns a legal `WHO_AM_I` (0x70/0x71/0x73) wins — zero configuration
> for either revision.

**I2C side: from "fixed addresses" to "scan the whole bus"**

The original implementation only probed `0x68` / `0x69` (the "standard" addresses set by AD0). Bench testing found that
some of the MPU6050 modules YaoKun sources are **marked with non-standard addresses** in their silkscreen and
datasheet, which a fixed table misses. It now scans the whole `0x08~0x77` range:

```cpp
for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() != 0) continue;   // no ACK, skip
    // ACK received: read WHO_AM_I(0x75)
    //   legal MPU id   → use it
    //   illegal id     → remember as a fallback candidate (possibly a domestic clone not in the table)
}
```

**Selection rule**: `0x68`/`0x69` have the highest priority (standard addresses are the most trustworthy); the rest in
ascending address order. If no legal id is found anywhere but some address does respond, fall back to the first
responding address and try the 6050 family (model=3) — `setupIMU()` automatically retries other models if `begin()`
fails, so it never dies in the probing stage.

**Bench result (both aircraft on the same firmware)**:

| Aircraft | Board | IMU module | Result |
| --- | --- | --- | --- |
| #1 | JLC ESP32-S3 R8N8 (COM28) | MPU6500 (SPI) | `MPU-6500 / SPI / WHO=0x70 / CS=17 / rotation −π/2` ✓ |
| #2 | Taobao ESP32-S3 N16R8 (COM24) | MPU6050 (I2C, non-standard address) | `MPU6050 / I2C0 / WHO=0x68 / SDA=9 SCL=8 / rotation +π/2` ✓ |

> 🔴 **Pitfall on record**: the I2C pins were once written as `SDA=8 / SCL=9` from faulty memory (the opposite of the
> pin table). Aircraft #1 has an SPI module and never touches I2C, so nothing showed up there; aircraft #2 with an I2C
> module could never be detected.
> **When the pin table and reality disagree, reality wins — but this time reality confirmed the table: `SDA=9 / SCL=8`.**

---

### R8 · LED and buzzer signalling (and the connection-criterion trap)

**LED priority** (high → low):

| Priority | Condition | Behaviour |
| --- | ---------- | ---------------------- |
| 1 | voltage < 3.5 V | red LED flashing at 100 ms + continuous fast beeping |
| 2 | connected and armed | green LED flashing at 200 ms |
| 3 | connected, not armed | green LED solid |
| 4 | not connected | red LED flashing at 500 ms |
| 5 | initialising | blue LED solid (driven directly in `setup()`) |

**Buzzer semantics** (identical to the YaoKun in-house controller YaoKunDrone, so users do not have to relearn them):

| Sound | Trigger | Meaning |
| ---------- | -------------- | ------- |
| short `'S'` | power-up | initialisation started |
| double `'D'` | init done / remote connected | ready |
| triple `'T'` | arming succeeded | armed |
| long `'L'` | RC lost / IMU unusable | alarm |
| continuous `'C'` | low battery | loops until voltage recovers |

**🔴 The connection-criterion trap (must read)**: the semantically equivalent upstream code uses `mavlinkTime` to decide
"ground station online", but `mavlinkTime` is refreshed by **any** MAVLink frame — ground-station heartbeats, parameter
requests, even the aircraft's own frames being looped back and counted. The result is "**connected even with no remote**"
— the LEDs lie.

This edition uses `controlTime` instead:

```cpp
const bool connected = valid(controlTime) && (t - controlTime < 1.0f);
```

`controlTime` is **only updated when `MANUAL_CONTROL` (a stick frame) arrives**, which is the true meaning of "remote
online". If SBUS is enabled, `readRC()` updates it too, so both paths are covered.

**Why the buzzer must be non-blocking**: calling `delay()` in the main loop to play a tone breaks the 1 kHz loop
immediately, the attitude estimate misses steps, and **the aircraft flips the instant you arm**. This is a real pitfall
we hit. So the design is "state machine driven by the main loop":

- `buzzerPlay(char)` only sets parameters and returns immediately
- `buzzerUpdate()` advances the timing from the main loop (the first thing `updateLED()` does)
- only the two tones inside `setup()` use `runBuzzerFor(ms)` to block briefly (arming is impossible then, so blocking is
  harmless)

---

### R9 · Fully Chinese UI (`i18n.h`)

**Design**: every UI string lives in `Firmware/flix/i18n.h` behind `TXT_*` macros, with `#ifdef FLIX_LANG_EN` selecting
a Chinese or English block. Chinese is the default; to build English, add one line at the very top of `flix.ino`.

**Why a separate file instead of scattering Chinese through the `.ino` files**:

1. The "compilable English firmware" path must survive (a GPL repository serves overseas users). With Chinese hard-coded
   in 30-plus `print()` calls, switching languages means editing line by line — easy to miss one, and the diff becomes
   enormous.
2. Once strings are centralised, "check whether any English is left in the UI" becomes **scriptable**: just verify every
   `print()` goes through `TXT_*`.
3. A future serial-display or web UI can reuse the same strings by including this one file.

**Convention**: macros expand to **string literals**, so write `print(TXT_XXX)`.
Parameterised messages are written as "prefix macro + format string" so both languages read naturally:

```cpp
print(TXT_CH_SET, ch);           // the prefix macro carries its own %d
print(TXT_PWM_SET " %d\n", freq);
```

**Verification**: `i18n.h` currently has **169** macros in each of the Chinese and English blocks; the script confirms
there is no one-sided entry (`-- 仅英文段有` / `仅中文段有` both empty). Two macros are defined but unreferenced
(`TXT_NO` / `TXT_YES`), which the checker reports explicitly.

> ⚠️ **Encoding requirement**: `i18n.h` must be saved as **UTF-8 without BOM**. If Notepad saves it as GBK, the Chinese
> turns into mojibake bytes and is sent straight to the serial port.

---

### R10 · Parameter table extensions

Eight new parameters; changes are persisted to NVS automatically, no rebuild needed:

| Parameter | Default | Description |
| --------------------------------------- | ----------- | ------------------------------------ |
| `IMU_AUTO_DETECT` | 1 | auto-detect the IMU at boot (1 = on, 0 = use the manual `IMU_MODEL` value) |
| `IMU_ROT_LOCK` | 0 | freeze the orientation so auto-detection stops overwriting it (recommended: set to 1 after `ca`) |
| `LED_R_PIN` / `LED_G_PIN` / `LED_B_PIN` | 41 / 7 / 14 | RGB LED pins |
| `LED_LOW_VOLT` | 3.5 | low-battery warning threshold (V) |
| `BUZ_PIN` | 39 | buzzer pin |
| `BUZ_ENABLE` | 1 | buzzer master switch (0 = silent, for indoor testing at night) |

> 🔴 **When to set `IMU_ROT_LOCK` to 1**: right after running `ca`. Auto-detection probes on every boot, and if one probe
> fails or reads a WHO_AM_I in an ambiguous range it may overwrite an orientation you already calibrated. Once locked,
> only `IMU_ROT_*` from the parameter table is used.

> 🔴 **Boards that carry NVS state must be flashed in segments**: a whole-chip write from `0x0` overwrites the nvs
> partition at `0x9000~0xE000` with 0xFF, i.e. erases it — the AP SSID, the detected IMU model and the RC calibration are
> all lost.
> The offsets come from `flix/build/<fqbn>/flash_args`.

---

### R11 · ESP-NOW + hotspot coexistence by default (2026-11-05)

**Why**: a replicator flashes the firmware and expects "the remote flies it and the phone shows telemetry" as soon as
it is assembled. The original build had three mutually exclusive modes; the ESP-NOW branch did bring up the hotspot
(ESP-NOW must live on an AP/STA interface) but two code paths shut the ground station out:

- `udp.begin()` was only called in the `W_AP` / `W_STA` branches → a ground station could join the hotspot and get no
  reply to anything;
- `sendWiFi()` did `if (espnow) { …; return; }` → telemetry only ever went out over ESP-NOW.

In other words **the physical layer could already coexist**; only these two software switches were missing.

**Changes** (`wifi.ino`):

| Location | Change |
| --- | --- |
| mode enum | new `W_BOTH = 4`, and the default is now `W_BOTH` |
| `udpStarted` | new flag, set only by `setupWiFi()`; the TX/RX paths use it to decide "can we use UDP", instead of testing the mode enum everywhere |
| `setupWiFi()` | `W_ESPNOW \|\| W_BOTH` share the RF init; `softAP(ssid, pass, espnowChannel)` sets the channel **once** (no second `setChannel`, which would kick already-connected clients); `W_BOTH` additionally calls `udp.begin()` |
| `sendWiFi()` | no early return after the ESP-NOW send; UDP is sent only when `udpStarted` **and** the station count > 0 (no clients, no airtime wasted) |
| `receiveWiFi()` | UDP is read only **after** the ESP-NOW queue is empty; the two byte streams never share a buffer (that would break streaming parsing) |
| `printWiFiInfo()` | mode name distinguishes `both`; `wifi` output also shows hotspot name / station count / IP, so you can tell at a glance whether the ground station is attached |
| `i18n.h` | `help` lists `both`; new `TXT_WIFI_MODE_BOTH` / `TXT_WIFI_COEX_MIGRATED` (one per language) |

**🔴 The NVS override trap (the biggest one in this change)**: `WIFI_MODE` is a persisted parameter, so
`setupParameters()` overwrites the compiled default with the NVS value. Old boards store `3` (ESP-NOW only), so
**changing only the default changes nothing** — the board reads 3 back on every boot. Hence a one-shot migration,
`migrateWifiModeToCoexist()`: an NVS marker `COEX_VER` makes it run once and upgrade 3 to 4; a user who later runs
`wifi espnow` explicitly is not switched back.
Out-of-range values are also rescued to `W_BOTH` — an illegal value makes `setupWiFi()` match no branch at all, which
shows up as "no hotspot after boot and the remote does not work either".

**SSID suffix now read from eFuse**: `WiFi.macAddress()` returns the AP MAC in AP context (aircraft #1 ends in
`62:02` → old name `YaoKun_6202`), which does not match the MAC printed by `wifi`. Using
`esp_read_mac(ESP_MAC_WIFI_STA)` makes the two agree (aircraft #1 → `YaoKun_F224`).
**There is no hard-coded suffix anywhere in the code**, so each board differs automatically.

**Dirty SSID validation (found in testing)**: aircraft #1 had the literal string `"<热点名>"` stored in
`WIFI_AP_SSID` (someone once typed the documentation placeholder into `ap <热点名> <密码>`), so auto-naming was skipped
and phones saw a hotspot literally called `<热点名>`. Validation added: empty name / contains `<` `>` / longer than 32
characters is treated as dirty → removed from NVS, auto-naming restored, and a notice printed.

**Command table**: `wifi ap/sta/espnow/both/off` — the first four keep exactly their old semantics, and `espnow` is
still pure ESP-NOW (no UDP), kept for bisecting airtime-interference problems.

**Verification**:

- Boot log: `无线模式：ESP-NOW + 热点（共存）` plus `当前热点名：YaoKun_xxxx`
- `wifi`: mode shows `both`; station count ≥ 1 once the phone joins
- The remote controls the aircraft **and** QGC simultaneously shows telemetry (each link independent)
- `接收丢弃字节数` should not climb noticeably just because the AP is up; if it does, airtime contention is severe — move
  closer or retest with `wifi espnow`

---

### R12 · The hotspot passphrase was overwritten by a placeholder (fixed 2026-11-05)

**Symptom**: a phone joining the aircraft hotspot always says "wrong password", with different phones and different
keyboards alike. The default in the code is byte-for-byte correct (`DEFAULT_AP_PASS` = `{49,50,…,56,0}`) and `p WIFI`
shows nothing unusual — superficially unsolvable.

**Root-cause chain** (three layers; missing any one makes it undiagnosable):

1. Someone years ago copied `ap <热点名> <密码>` from the documentation verbatim;
2. At that time `configWiFi()` had **exactly one** validation for the passphrase: `strlen(second) >= 8`;
3. The UTF-8 encoding of `<密码>` is `60,229,175,134,231,160,129,62` — **exactly 8 bytes** → it passed validation →
   was written to `WIFI_AP_PASS` in NVS, and the Wi-Fi driver wrote it into the net80211 namespace as `ap.passwd`.

From then on every boot overwrote the compiled default with the NVS value — **editing code or reflashing the app
partition changed nothing**; only a whole-chip write (clearing NVS) or resetting the passphrase could recover. NVS raw
evidence: at `0x024E8`, `ap.passwd = [60,229,175,134,231,160,129,62]`.

**Fix (three layers of defence; all three are required)**:

| Layer | Location | What it does |
|---|---|---|
| read | `setupWiFi()` | `isUsableWifiPass()` validates the NVS passphrase; unusable → `storage.remove()` + fall back to the factory default + print a notice |
| write | `configWiFi()` | validate before `ap` / `sta` writes; illegal → reject and explain (`TXT_WIFI_ARG_INVALID`) |
| source | `DEFAULT_AP_PASS` | factory passphrase is now built from the ASCII byte table `{49,50,51,52,53,54,55,56,0}`; no digit string appears in the source (passphrase = `1-2-3-4-5-6-7-8`, hyphens are separators only and are not typed) |

**The rules inside `isUsableWifiPass()` and why**:

| Rule | Rationale |
|---|---|
| length 8~63 | a hard 802.11 constraint for WPA2-PSK passphrases; below 8 the core rejects it (silently becomes an open network), above 63 is truncated |
| printable ASCII only (0x20~0x7E) | non-ASCII passphrases depend on keyboard/IME/encoding and cannot be debugged in the field; this factory passphrase is numeric anyway |
| no `<` `>` | the signature of a documentation placeholder — this incident came from exactly that |

**Measured result** (aircraft #1, after `ap YaoKun_F224 <factory passphrase>`):

- board side: `wifi` shows `热点名称：YaoKun_F224`, `已连设备数：0`, `本机 IP 192.168.4.1`
- all three NVS locations hold `[49,50,51,52,53,54,55,56]` (`WIFI_AP_PASS` + the driver's `ap.passwd` ×2)
- PC side: full-band scan with the original network disconnected shows `YaoKun_F224` → `netsh wlan connect` →
  **status "connected", WPA2-Personal, signal 99 %, BSSID `82:65:99:df:f2:24`** ⇒ the passphrase path is correct end to end

**Debugging mnemonic**: `p WIFI` deliberately does not echo the passphrase. To tell whether it was sanitised, look for
"NVS 里的热点密码不可用…已恢复出厂默认" (the NVS passphrase is unusable … factory default restored) in the boot log.

---

## 4. Verification flow (three levels)

### Level 1 · Static (no hardware needed)

| Check | Method | Pass criterion |
| ----- | ------------------------------------------- | --------------------- |
| compiles | `arduino-cli compile ... --export-binaries` | exit code 0, no warnings |
| zh/en pairing | `python Firmware/tools/check_i18n.py` | **169 : 169**, no one-sided entries |
| English build works | rebuild with `-DFLIX_LANG_EN` | exit code 0 |
| no English leftovers | same script · scans `print("` calls not using `TXT_` | only pure format strings and CSV headers remain |
| footprint | compiler output | Flash < 90 %, RAM < 60 % |

**Actual results (2026-10-18)**:

| Item | Chinese build | English build |
| ---------- | ---------------- | ----------- |
| compile | EXIT=0, 0 warnings | EXIT=0 |
| Flash (app) | 1,052,064 B (80 %) | 1,050,804 B |
| RAM (static) | 110,140 B (33 %) | — |
| zh/en pairing | ✔ 169 : 169 | ✔ 169 : 169 |

> `check_i18n.py` runs five checks: zh/en pairing / undefined references / English that bypasses i18n / BOM /
> control characters.

### Level 2 · Bench (**propellers must be removed**)

| Step | Command / action | Expected result |
| -- | ---------------- | ---------------------------------- |
| 1 | power up, watch the first screen | Chinese banner + fingerprint string + `固件版本：v1.1` |
| 2 | `imu` | model / bus / WHO_AM_I / orientation correct; `状态：正常` |
| 3 | `ca` | six Chinese prompts, 8 s each, last line `校准完成！` |
| 4 | `st`, tilt the airframe | right side down → `横滚` positive (**the only orientation criterion**) |
| 5 | power the remote, `rc` | `原始通道` follows the sticks; `最后收到遥控的时刻` keeps refreshing |
| 6 | `ch` | `ESP-NOW 信道：1` (must match the remote) |
| 7 | `wifi` | `原生接收` = 1; `接收丢弃字节数` barely moves |
| 8 | stick at min / centre / max | normalised throttle should be **-1 / 0 / +1** (see "range semantics" below) |
| 9 | `mfr 0.1` on each motor | correct direction and position (**remove propellers**) |
| 10 | `arm` / `disarm` | triple beep on arm / LED switches to green flashing |
| 11 | cut power to the remote | long beep + slow red flashing after 1 s |

**⚠️ MANUAL_CONTROL range semantics (must check on the bench)**:
the protocol defines `x/y/z/r` as int16 with a full range of **±1000**. This project uses
`controlThrottle = m.z / 1000.0f`, so z spans **-1000 ~ +1000** with 0 at centre.
→ If the remote actually sends **0 ~ 1000** (treating centre as 0), the stick centre is already 0 throttle and thrust
only appears in the upper half — which feels like "only half the travel, and it jumps at mid-stick".
**On the first bench session you must confirm: stick at minimum → z ≈ -1000, centre → z ≈ 0, maximum → z ≈ +1000.**

### Level 3 · Tethered low hover

1. Fit propeller guards, tether the aircraft, use open space
2. `arm`, raise the throttle slowly until it lifts 10~20 cm
3. Tap roll/pitch and confirm the **correction direction is right** (tilt right → the aircraft corrects right)
4. Release the sticks and confirm the attitude levels out (**but it keeps drifting horizontally** — expected, there is
   no position sensor)
5. Watch the LEDs throughout: green flashing = armed, red flashing = link lost, fast red = low battery

---

## 5. Known limitations

| Limitation | Detail |
| -------------- | ---------------------------------------------- |
| **No altitude hold** | no barometer or rangefinder; releasing the throttle loses altitude |
| **No position hold** | no GPS or optical flow; releasing the sticks does not hover, the aircraft keeps drifting at its current horizontal velocity |
| **No automatic landing** | only `rcLossFailsafe` (1 s: throttle down, 10 s: zero and disarm), not a controlled descent |
| **Single app partition** | `PartitionScheme=default` has no OTA; upgrading means flashing over a cable |
| **Console over native USB** | `CDCOnBoot=cdc`; no log if you only connect the UART header |
| **Two aircraft interfere** | same-channel ESP-NOW + same SSID; power the other one down while testing |

---

## 6. Troubleshooting quick reference

| Symptom | Most likely cause | How to confirm |
| --------------- | ------------------------ | ----------------------------- |
| remote says connected, aircraft does not move | ESP-NOW channel mismatch / native receive not registered | `ch` for the channel; `wifi` for whether `原生接收` is 1 |
| remote says connected, aircraft does not move | remote sends 0~1000 instead of ±1000 | `rc` for the normalised throttle range |
| all telemetry stops 71.6 min after power-up | R1 not in effect (old firmware flashed) | `time` for `回绕次数`; verify the SHA-256 |
| red LED solid + long beep at boot | IMU detection failed | `imu` for `识别依据` and the pins |
| crashes / reboots at boot | wrong board firmware (IMU model mismatch) | check the first screen's `识别为 …` against the actual chip |
| attitude inverted (tilting right shows left) | wrong IMU orientation bracket | `st` while tilting: right side down must give **positive** roll |
| arming rejected | throttle not at minimum | console prints `拒绝解锁：油门不在最低位` |
| phone disconnects, UI freezes | router and aircraft AP on the same channel | separate them with `ch`, change the remote to match |
| packaged firmware is wrong | compiled without `--export-binaries` | compare the modification time of the bin in the build directory |

---

## 7. Compatibility with upstream

| Item | Status |
| --------------------- | ------------------------------------------------- |
| `master` branch | byte-identical to upstream, can be fast-forwarded at any time |
| `yaokun-organized` branch | this branch; it does not follow upstream automatically |
| merging upstream changes | use `gh/sync_upstream.py --check` to see the diff, then move changes across by path |
| GPL obligations | keep `(C) Oleg Kalachev` and the upstream repository URL; YaoKun information is appended after it |
| upstream LICENSE | **flix ships no separate LICENSE file** ⇒ copyright stays with the original author, **do not declare a different licence** |

---

## 8. Find us

- Bilibili: [耀坤智联](https://space.bilibili.com/3707011699050767)
- OSHWHub (open-source hardware): [耀坤无人机](https://oshwhub.com/lindahe)
- GitHub: <https://github.com/YaoKunS>
- Taobao store: [耀坤智联](https://shop35432590.taobao.com/)

---

*This document corresponds to firmware version v1.1. If you change the code, update the matching section.*
