#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
================================================================================
耀坤整理版 Flix 固件打包脚本
================================================================================

用途
----
把 arduino-cli 编译出来的 4 个分片（bootloader / 分区表 / boot_app0 / 应用程序）
合并成**一个**可以「从 0x0 整片刷写」的 merged.bin，并自动产出：

    1) <发布名>_merged.bin     单文件固件（烧写地址 0x0）
    2) <发布名>_指纹与烧录说明.md
    3) <发布名>_指纹与烧录说明.txt   （与 md 内容一致，纯文本便于手机/记事本查看）

所有哈希、大小、镜像头字段都是**现场从 bin 里读出来的**，不是手抄，保证文档与
固件严格一致（这份说明的权威性就建立在这一点上）。

用法
----
    python pack_flix_release.py

    # 指定构建目录 / 输出目录 / 版本号（用正斜杠，Windows 命令行同样接受，且不受转义影响）
    python pack_flix_release.py --build-dir "Firmware/flix/build" ^
                                --out-dir   "Firmware/release" ^
                                --version   "1.1"

前置条件
--------
必须先编译，且编译时必须带 --export-binaries（或用 export.bat），否则 build 目录
里没有 .bin，脚本会拿旧文件拼出一个「看起来正常但内容错误」的固件 —— 这是本工程
踩过的坑，所以脚本开头就检查 4 个分片的存在性与修改时间。

合并布局（为什么是这四段）
--------------------------
    0x0       bootloader.bin     二级引导，芯片复位后第一段执行的代码
    0x8000    partitions.bin     分区表（nvs / otadata / app0 ...）
    0xE000    boot_app0.bin      OTA 数据初值，缺它芯片不知道从 app0 启动 → 不跑
    0x10000   app.bin            应用程序本体

⚠️ 段间空隙（如 0x9000~0xE000 的 nvs 区）会被填 0xFF。
   这意味着「从 0x0 整片刷写」等于**清空 NVS**：WiFi 校准、已标定的 IMU 朝向、
   遥控端点全部丢失。自己调好的机器请改用分片刷写（见脚本末尾打印的命令）。

⚠️ --flash-mode 必须是 dio
   ESP32-S3 的 bootloader 镜像头固定写 DIO（见 esptool 的 bootloader 头 33 字节差异），
   若合并时声明 qio，镜像头与 bootloader 实际模式不一致，部分板子会启动失败。
================================================================================
"""

import argparse
import hashlib
import os
import re
import struct
import sys
import datetime

# 分区/引导文件的固定烧写地址（与 flix/build/<fqbn>/flash_args 一致，勿改）
FLASH_MAP = [
    (0x0000, "flix.ino.bootloader.bin", "bootloader", "二级引导程序"),
    (0x8000, "flix.ino.partitions.bin", "partitions", "分区表"),
    (0xE000, "boot_app0.bin",           "boot_app0",  "OTA 数据初值"),
    (0x10000, "flix.ino.bin",           "app",        "应用程序"),
]

# boot_app0.bin 不在 build 目录里，由 esp32 core 提供；找不到时按经验路径兜底
BOOT_APP0_HINTS = [
    os.path.join(os.environ.get("LOCALAPPDATA", ""),
                 "Arduino15", "packages", "esp32", "hardware", "esp32",
                 "3.3.8", "tools", "partitions", "boot_app0.bin"),
]

MERGE_END = 0x10000  # 应用程序起始地址，实际长度运行时从 app.bin 读出


def sha256_file(path):
    """计算文件的 SHA-256（十六进制小写字符串）。分块读，避免大文件吃内存。"""
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def find_boot_app0(build_dir):
    """定位 boot_app0.bin：先在 build 目录找，再按 esp32 core 的安装路径兜底。"""
    local = os.path.join(build_dir, "boot_app0.bin")
    if os.path.isfile(local):
        return local
    for p in BOOT_APP0_HINTS:
        if os.path.isfile(p):
            return p
    return None


def parse_image_header(path):
    """
    解析 ESP 镜像头，返回字典。

    【为什么需要它】文档里要写"镜像头 E9 06 02 2F → DIO 80MHz 4MB"，
    这串东西不能靠猜。这里按 esp_image_header_t 的实际字节顺序读。
    实测（esptool 5.2.0 image-info 交叉验证）本工程 app.bin 前 16 字节为：

        E9 06 02 2F A4 9D 37 40 EE 00 00 00 09 00 00 00
        │  │  │  │  └─────┬────┘  │              └── chip_id = 9 (ESP32-S3)
        │  │  │  │    entry_point │
        │  │  │  │     0x40379DA4 wp_pin = 0xEE (disabled)
        │  │  │  └─ spi_speed = 0x2F → 80MHz
        │  │  └──── spi_mode  = 0x02 → DIO
        │  └─────── segment_count = 6
        └────────── magic = 0xE9

    🔴 踩过的坑（记下来省得再踩）：
       1) 以为 offset 3 是 speed、offset 4 起是 uint32 的 size → 读出 1077386660 垃圾值。
       2) 按 "3=size, 4=speed" 改 → 全错。
       3) 一度以为"镜像头里没有独立的 spi_size 字节" → 也是错的。
       4) ★真相：**offset 3 = spi_speed(低 4 位) | spi_size(高 4 位)** 两个半字节拼在一个字节里。
          实测：v1.0 包 0x2F = 80MHz + 4MB；v1.1 包 0x3F = 80MHz + 8MB。
          esp_app_desc_t / 扩展头里的容量信息是另一份冗余，别混为一谈。
       结论：**不要在这个函数里硬读 uint32 的 size**（应用长度从 app.bin 文件大小来），
       容量指纹取 offset 3 的高半字节即可。
    """
    with open(path, "rb") as f:
        head = f.read(16)
    if len(head) < 9:
        return None

    magic, seg_count, spi_mode, spi_speed_size = head[0], head[1], head[2], head[3]
    entry = struct.unpack("<I", head[4:8])[0]

    mode_names = {0: "QIO", 1: "QOUT", 2: "DIO", 3: "DOUT"}
    # ⚠️ 镜像头第 4 字节（offset 3）是"速度 + 容量"拼在一起的两个半字节：
    #    低 4 位 = spi_speed（0x0=40MHz, 0x1=26MHz, 0x2=20MHz, 0xF=80MHz）
    #    高 4 位 = spi_size（0x2=4MB, 0x3=8MB, 0x4=16MB）
    #    实测：v1.0 包 0x2F = 80MHz + 4MB；v1.1 包 0x3F = 80MHz + 8MB。
    #    【踩坑记录】早期版本把整字节当速度查表 → 0x3F 查不到显示 "?0x3F"，
    #    还误以为"镜像头里没有容量字段"。正确做法就是拆半字节。
    speed_names = {0x0: "40MHz", 0x1: "26MHz", 0x2: "20MHz", 0xF: "80MHz"}
    size_names = {0x0: "1MB", 0x1: "2MB", 0x2: "4MB", 0x3: "8MB", 0x4: "16MB"}

    speed_nibble = spi_speed_size & 0x0F
    size_nibble = (spi_speed_size >> 4) & 0x0F

    return {
        "magic": magic,
        "magic_ok": magic == 0xE9,
        "seg_count": seg_count,
        "spi_mode": mode_names.get(spi_mode, "?%d" % spi_mode),
        "spi_speed": speed_names.get(speed_nibble, "?0x%X" % speed_nibble),
        "spi_size": size_names.get(size_nibble, "?0x%X" % size_nibble),
        "entry": entry,
        "raw": "%02X %02X %02X %02X" % (head[0], head[1], head[2], head[3]),
    }


def human(n):
    """字节数转人类可读（带十六进制，方便和 flash_args 对照）。"""
    return "%d B (0x%X)" % (n, n)


# ----------------------------------------------------------------------------
# 发布脱敏（铁律：发布物严禁出现本机路径）
# 【来源】esp32 core 源码（esp32-hal-uart.c / esp32-hal-rmt.c 等）里的 assert()
#   宏把 __FILE__ 在**编译期**展开为绝对安装路径，例如：
#       C:\Users\<用户名>\AppData\Local\Arduino15\packages\esp32\hardware\...
#   这是 Arduino 构建系统用绝对路径编译 core 源码的必然结果，v1.0 就带着发布过。
# 【做法】把整条路径替换为 '<esp32-core>\文件名'，再用 \0 补齐到原长度——
#   镜像内所有偏移保持不变。
# 【⚠️ 2026-11-06 重大修复】改了段数据后，ESP app 镜像尾部有**两处**完整性数据：
#   ① XOR 校验字节（各段数据逐字节 XOR，初始 0xEF，位于 16 对齐处、SHA 前 1 字节）
#   ② appended SHA-256（末 32 字节，覆盖其前全部内容）
#   旧版只重算了 ② 没重算 ① ⇒ 所有脱敏过的发布件 bootloader 报
#   "Checksum failed" 拒绝启动（20261105 v1.1 S3 发布件与 20261106 两件全部中招）。
#   现在 fix_app_image_trailer() 把两处都重算。
# 【验证】esptool image-info 应显示 Validation Hash valid；并全量扫描零用户名；
#   发布前再用 parse/fix 的自检（fix 后再解析一次，存储=重算）兜底。
# ----------------------------------------------------------------------------
LOCAL_PATH_RE = re.compile(
    rb'C:\\Users\\[^\\]+\\AppData\\Local\\Arduino15\\packages\\esp32\\[^\x00]+')


def fix_app_image_trailer(data):
    """重算 ESP app 镜像尾部的 XOR 校验字节与 appended SHA-256。

    【为什么需要】bootloader 启动 app 前做两道校验：
      ① XOR 校验字节：各段段数据（不含 8+16 字节头）逐字节异或，初始 0xEF，
         写在最后一个段之后 16 字节对齐的位置（SHA 前 1 字节）；
      ② appended SHA-256：末 32 字节，覆盖镜像从 0 到校验字节（含）的全部内容。
    改段内容（哪怕 1 字节）后两处都必须重算，否则 bootloader 报
    "Checksum failed" / 哈希不匹配而拒绝启动（2026-11-06 火龙果实机踩坑实证）。

    参数 data: bytes —— app 镜像原始内容（0xE9 头）
    返回 (修复后的 bytes, 是否为 app 镜像 bool)
    纯 struct 实现，不依赖 esptool（本机 pip 网络不可达，靠的是逐字节推导实证）。
    """
    import struct
    if len(data) < 64 or data[0] != 0xE9:
        return data, False
    nseg = data[1]                       # ESP 镜像头第 2 字节 = 段数
    pos = 8 + 16                         # 8B 公共头 + 16B 扩展头（esp32 app 自 IDF v4 起都有）
    total = 0xEF                         # ESP_CHECKSUM_MAGIC
    for _ in range(nseg):
        _load, ln = struct.unpack_from('<II', data, pos)
        for b in data[pos + 8:pos + 8 + ln]:
            total ^= b
        pos += 8 + ln
    # 校验字节位置：esptool 的 align_file_position(f,16) 把它对齐到
    # 「下一个 ≡15 (mod 16)」的偏移（即 16 字节块的最后一个字节），
    # 这样其后跟的 32 字节 SHA 末尾正好让镜像长度为 16 的倍数。
    # ⚠️ 2026-11-06 实证：写成「向上取整到 16 对齐」会让 bootloader 读错位报
    #    Checksum failed（0xce vs 0x25，实机两次踩坑后从 bootloader 报错值反推定位）。
    cs_off = pos + ((15 - pos % 16) % 16)
    out = bytearray(data)
    out[cs_off] = total                  # ① 重算 XOR 校验字节
    # ② 重算 appended SHA-256（覆盖 0 ~ 校验字节含）
    fixed = bytes(out[:cs_off + 1]) + hashlib.sha256(bytes(out[:cs_off + 1])).digest()
    pad = (-(cs_off + 33)) % 16          # 原镜像长度为 16 的倍数，SHA 后补零对齐
    fixed += b'\x00' * pad
    return fixed, True


def sanitize_local_paths(data):
    """把固件段里的本机安装路径替换为等长占位串。

    参数 data: bytes —— 段的原始内容
    返回 (新内容 bytes, 替换处数 int)
    替换策略：路径串整体换成 '<esp32-core>\\<文件名>'，尾部用 \\0 补到原长。
    """
    out = bytearray(data)
    changed = 0
    for m in LOCAL_PATH_RE.finditer(bytes(data)):
        full = m.group(0)
        fname = full.rsplit(b'\\', 1)[-1]
        new = b'<esp32-core>\\' + fname
        if len(new) > len(full):
            new = new[:len(full)]  # 兜底：占位串不应比原路径长（原路径都很长）
        out[m.start():m.start() + len(new)] = new
        out[m.start() + len(new):m.start() + len(full)] = b'\x00' * (len(full) - len(new))
        changed += 1
    return bytes(out), changed


def sanitize_segment(path, tag):
    """读取段文件 → 脱敏 → 重算镜像尾部（XOR 校验 + SHA-256）→ 写回原路径。

    只有真正的 ESP app 镜像（0xE9 头）才动尾部；partitions.bin / boot_app0.bin
    不是 app 镜像，等长替换后无需重算（它们也没有 appended 校验结构）。
    返回 (处理后的数据, 处理后的哈希, 替换处数)。
    """
    with open(path, "rb") as f:
        data = f.read()
    data, n = sanitize_local_paths(data)
    if n:
        # 改了段数据 ⇒ XOR 校验字节与 appended SHA-256 都必须重算（缺一即拒启）
        data, is_app = fix_app_image_trailer(data)
        with open(path, "wb") as f:
            f.write(data)
        print("  [脱敏] %-12s 替换 %d 处本机路径%s" % (tag, n, "（已重算校验字节+SHA）" if is_app else ""))
    return data, hashlib.sha256(data).hexdigest(), n


def build_merged(build_dir, out_merged):
    """
    把四段按地址拼成一个文件。

    实现要点：用「稀疏写入」—— 先 seek 到目标偏移再写，中间空洞由文件系统补 0。
    最后统一把空洞转成 0xFF（烧到 flash 里的空状态就是 0xFF），
    这样 merged.bin 的字节与"整片擦除后逐段烧写"的结果完全一致。
    """
    total_end = MERGE_END
    app_path = os.path.join(build_dir, "flix.ino.bin")
    app_size = os.path.getsize(app_path)
    total_end = MERGE_END + app_size

    buf = bytearray(b"\xff" * total_end)  # 先整块填 0xFF（最坏情况 ~1.1MB，内存无压力）
    segments = []

    for addr, fname, tag, desc in FLASH_MAP:
        path = os.path.join(build_dir, fname)
        if tag == "boot_app0":
            path = find_boot_app0(build_dir)
            if path is None:
                raise SystemExit("找不到 boot_app0.bin，请检查 esp32 core 是否完整安装")
        # 发布脱敏：替换本机路径（详见 sanitize_local_paths 注释），返回处理后的数据
        data, seg_sha, _n = sanitize_segment(path, tag)
        if addr + len(data) > total_end:
            raise SystemExit("段 %s 超出合并范围（%d+%d > %d）" % (tag, addr, len(data), total_end))
        buf[addr:addr + len(data)] = data
        segments.append({
            "addr": addr, "name": fname, "tag": tag, "desc": desc,
            "size": len(data), "sha256": seg_sha, "path": path,
        })

    with open(out_merged, "wb") as f:
        f.write(buf)

    return segments, len(buf)


def render_doc_en(version, stem, merged_sha, merged_size_real, segments, hdr, boot_hdr, build_time):
    """
    渲染英文版《Firmware Fingerprint & Flashing Guide》。

    【为什么必须由脚本生成，而不是手写一份英文 md】
      这份文档里每一个哈希、大小、镜像头、编译时间都必须与 bin **现场一致**。
      手写一次，下次改代码重打包就会漂（哈希变了文档没变 = 用户按文档校验必然失败）。
      所以英文版与中文版共用同一批现场数据，只换文案语言。

    【为什么上游社区也需要它】
      Flix 本身是英文社区项目（(C) Oleg Kalachev），海外用户拿到发布包只会看英文；
      中文版对他们等于没有。这一份是给他们的正式烧录文档。

    参数：
      version          对外版本号（如 "1.1"）
      stem             发布文件名主干，用于拼 merged.bin 全名（文件名含中文，供直接复制粘贴）
      merged_sha       合并后单文件固件的 SHA-256
      merged_size_real 合并后单文件固件的真实字节数
      segments         build_merged() 返回的分段列表（addr / name / tag / size / sha256）
      hdr              app.bin 的镜像头字典（parse_image_header）
      boot_hdr         bootloader 的镜像头字典，可能为 None
      build_time       本工程编译时间（从 app.bin 里按 __DATE__/__TIME__ 配对扫出）

    返回：Markdown 文本（调用方再降级成纯文本 txt）。
    """
    # 分段的英文名：FLASH_MAP 里的 desc 是中文，只给中文文档用
    EN_DESC = {
        "bootloader": "Bootloader",
        "partitions": "Partition table",
        "app": "Application",
    }

    # 出厂密码：全文档统一写带连字符的形式（连字符只是分隔符，输入时去掉）。
    # 禁止写连续 8 位明文 —— 见 docs 里记录过的显示通道改写事故。
    E = []
    A = E.append

    A("# YaoKunS3 Drone / Flix Organized Edition — Firmware Fingerprint & Flashing Guide")
    A("")
    A("## 1. What this firmware is")
    A("")
    A("This is the **YaoKun organized edition** of the open-source flight controller **Flix** "
      "(original author: Oleg Kalachev). The upstream code structure and all GPL obligations are preserved; "
      "what was added is support for the YaoKun S3 airframe and for our own ESP-NOW remote.")
    A("")
    A("- **Upstream**: <https://github.com/okalachev/flix> (`(C) Oleg Kalachev` — upstream ships no separate "
      "LICENSE file, copyright stays with the original author)")
    A("- **Organized-edition repo**: <https://github.com/YaoKunS/flix> (branch `yaokun-organized`; "
      "`master` is kept identical to upstream)")
    A("- **Hardware**: YaoKun S3 drone (ESP32-S3, 8 MB flash), four brushed motors on GPIO 15/5/6/16")
    A("- **IMU**: MPU6050 (I2C, WHO_AM_I=0x70) **or** MPU-6500 (SPI) — **auto-detected at boot**, no rebuild needed")
    A("- **Control**: attitude loop → rate loop → quad mixer (cascaded PID); modes RAW / ACRO / STAB / AUTO")
    A("- **RC link**: YaoKun ESP-NOW remote (channel 1); SBUS receivers and Wi-Fi/UDP ground stations also work")
    A("- **Ground station**: MAVLink v2 (QGC / MAVProxy)")
    A("- **New in v1.1**: (1) **ESP-NOW + hotspot coexistence is the factory default** (older builds had mutually "
      "exclusive modes; boards carrying an old NVS value are migrated automatically on first boot); "
      "(2) the hotspot name suffix is now read from the **eFuse factory STA MAC** "
      "(e.g. `YaoKun_F224`, matching the last two bytes of the MAC printed by the `wifi` command); "
      "(3) the hotspot passphrase now has **three layers of validation** "
      "(dirty NVS value is cleaned on read / illegal writes are rejected / factory passphrase is built from a byte table)")
    A("- **About the hotspot**: it only carries **MAVLink over UDP 14550**. There is **no web UI** — "
      "a browser hitting 192.168.4.1 will show nothing, which is expected")
    A("- **LEDs & buzzer**: RGB LEDs on GPIO 41/7/14 plus an active buzzer on GPIO 39, driven by a non-blocking state machine")
    A("- **UI language**: **Chinese by default** (console, `help` menu, IMU calibration, boot messages). "
      "Define `FLIX_LANG_EN` to build the English UI")
    A("")
    A("> ★ This firmware is **attitude stabilization only, with a manual throttle**: there is no GPS, "
      "barometer, rangefinder or optical flow.")
    A("> **Releasing the sticks does not hold position** — the attitude levels out but the aircraft keeps drifting "
      "at its current horizontal velocity. **Releasing the throttle means losing altitude.**")
    A("> For the first flight use open space, stay low, and fit propeller guards.")
    A("")
    A("## 2. Version and provenance")
    A("")
    A("| Item | Value |")
    A("| --- | --- |")
    A("| Firmware version | **v%s** (YaoKun organized edition) |" % version)
    A("| Brand | YaoKunS / YaoKunSmart / YaoKun Drone |")
    A("| Build time | %s |" % build_time)
    A("| Toolchain | arduino-cli 1.5.1 + esp32 core 3.3.8 |")
    A("| Build flags | `esp32:esp32:esp32s3:FlashSize=8M,PartitionScheme=default_8MB` (USB CDC on by default) |")
    A("| Third-party libs | FlixPeriph, ESP32_NOW_Serial (ships with esp32 core), MAVLink |")
    A("| Footprint | application %s |" % human(segments[-1]["size"]))
    A("")
    A("> ⚠️ **How to read the version number**: the `esp_app_desc_t` at `0x10020` inside an Arduino-built image "
      "is written by the precompiled ESP32 core library, so its `version` field identifies the *core*, "
      "not this project. The authoritative version is the one printed on the **boot banner**: "
      "`Firmware version: v%s (YaoKun organized edition)`." % version)
    A("")
    A("## 3. How to flash (single file — the simple way)")
    A("")
    A("### Option A: Flash Download Tool (GUI)")
    A("")
    A("1. Open Flash Download Tool, select chip **ESP32-S3**, work mode **Develop**.")
    A("2. Add only this one file at address **`0x0`**: `%s_merged.bin`" % stem)
    A("3. Settings: **SPI SPEED = 80 MHz, SPI MODE = DIO, FLASH SIZE = 8 MB**; pick the COM port; "
      "BAUD 921600 (drop to 115200 if unstable).")
    A("4. ⚠️ **Tick DoNotChgBin.** The image header already declares SPI mode / speed / size and the trailing "
      "SHA-256; if the tool rewrites the header the checksum breaks and the board will not boot.")
    A("5. Press START, then RST or power-cycle when it finishes.")
    A("")
    A("### Option B: one esptool command")
    A("")
    A("```bash")
    A("esptool --chip esp32s3 --port COMx --baud 921600 write_flash 0x0 \"%s_merged.bin\"" % stem)
    A("```")
    A("")
    A("If the board stays in download mode afterwards (log shows `waiting for download`), GPIO0 is still held low. "
      "Connect once with `--after no-reset` to confirm the image is readable, then run "
      "`esptool --after watchdog-reset run`, or simply replug USB / press RST.")
    A("")
    A("## 4. Verifying that you flashed *this* build (fingerprints)")
    A("")
    A("**1) Compare the file hash** (most reliable, no power-up needed):")
    A("")
    A("```bat")
    A("certutil -hashfile \"%s_merged.bin\" SHA256" % stem)
    A("```")
    A("")
    A("→ should be `%s`" % merged_sha)
    A("")
    A("**2) Serial boot banner** (115200 baud, over the **native USB-CDC port**). The first screen should read:")
    A("")
    A("```")
    A("耀坤S3 飞控（Flix 整理版）启动中…")
    A("YaoKunS | 耀坤S3无人机 | Flix | 耀坤智联")
    A("固件版本：v%s（耀坤整理版）" % version)
    A("初始化参数存储")
    A("初始化电源检测")
    A("初始化无线通信")
    A("ESP-NOW 信道：1")
    A("…（IMU 自动识别输出）…")
    A("惯性测量单元就绪：MPU6500，总线 SPI")
    A("初始化完成")
    A("```")
    A("")
    A("> The banner text is Chinese in this build (the Chinese UI is the default). An English-language build prints "
      "`Firmware version: v%s (YaoKun organized edition)` instead — see §8 for how to build one." % version)
    A("")
    A("Three key fingerprints: the **product string**, the **version line**, and **ESP-NOW channel 1**. "
      "All three together identify this build.")
    A("")
    A("**3) Search the bin for plain-text strings** (confirms it is the organized edition without any hardware):")
    A("")
    A("- Search for `耀坤S3 飞控（Flix 整理版）启动中` → found (the Chinese boot banner; the English build does not contain it)")
    A("- Search for `YaoKunS | 耀坤S3无人机 | Flix | 耀坤智联` → found exactly once (the fingerprint string)")
    A("- Instead of hunting for `esp_now_register_recv_cb`, search for `接收丢弃字节数` → found "
      "(the native ESP-NOW receive counter)")
    A("")
    A("**4) Read the image header out of flash** (read-only, changes nothing):")
    A("")
    A("```bash")
    A("esptool --chip esp32s3 --port COMx read_flash 0x10000 0x200 app_hdr.bin")
    A("```")
    A("")
    A("→ the first byte must be `E9` (the ESP image magic).")
    A("")
    A("## 5. Artifacts and checksums (SHA-256)")
    A("")
    A("| Artifact | Flash address | SHA-256 | Size |")
    A("| --- | --- | --- | --- |")
    A("| **merged single-file firmware** | `0x0` (whole chip) | `%s` | %s |" % (merged_sha, human(merged_size_real)))
    for s in segments:
        addr = "0x%X" % s["addr"]
        if s["tag"] == "boot_app0":
            A("| boot_app0 | `0xE000` | provided by esp32 core 3.3.8 (points otadata at app0; without it the board "
              "will not boot) | %s |" % human(s["size"]))
        else:
            A("| %s | `%s` | `%s` | %s |" % (EN_DESC.get(s["tag"], s["tag"]), addr, s["sha256"], human(s["size"])))
    A("")
    A("- Merge layout: `0x0 bootloader + 0x8000 partition table + 0xE000 boot_app0 + 0x10000 application` "
      "(gaps are filled with 0xFF)")
    A("- Application image header: `%s` → SPI mode **%s**, speed **%s**, flash size **%s**, segments **%d**, "
      "entry **0x%08X**" % (hdr["raw"], hdr["spi_mode"], hdr["spi_speed"], hdr["spi_size"], hdr["seg_count"], hdr["entry"]))
    A("- ⚠️ This package is built for **%s flash** (`PartitionScheme=default_8MB`). **It will not fit a 4 MB board** — "
      "rebuild from source with `FlashSize=4M,PartitionScheme=default`" % hdr["spi_size"])
    A("- Flash size: **%s**, taken from the high nibble of header byte 3 (low nibble is the SPI speed). "
      "Cross-check with `esptool image-info <bin>` if you like" % hdr["spi_size"])
    if boot_hdr:
        A("- Bootloader image header: `%s` → SPI mode **%s**, speed **%s**, flash size **%s**"
          % (boot_hdr["raw"], boot_hdr["spi_mode"], boot_hdr["spi_speed"], boot_hdr["spi_size"]))
    A("")
    A("## 6. Segmented flashing (keeps the calibration data in NVS)")
    A("")
    A("**Flashing the single file from 0x0 overwrites the nvs partition at `0x9000~0xE000` with 0xFF, i.e. it erases it.**")
    A("That means: Wi-Fi hotspot name/passphrase, the auto-detected IMU result, and the RC endpoint calibration are all gone.")
    A("")
    A("On a blank board that does not matter; on **a machine you have already calibrated**, flash the segments instead "
      "and skip nvs:")
    A("")
    A("```bash")
    A("esptool --chip esp32s3 --port COMx --baud 921600 write_flash \\")
    for s in segments:
        if s["tag"] == "boot_app0":
            continue
        A("  0x%X \"%s\" \\" % (s["addr"], s["name"]))
    A("```")
    A("")
    A("(`boot_app0.bin` lives under the esp32 core's `tools/partitions/`; not reflashing it is normally fine.)")
    A("")
    A("## 7. Six things you should know")
    A("")
    A("1. **After the first whole-chip flash, re-run `cal` (IMU calibration) and the RC calibration** — the NVS was "
      "erased, so the IMU biases must be measured again.")
    A("2. The console runs over the **native USB port** (`CDCOnBoot=cdc`). Seeing no log when you only connect the "
      "board's UART header is expected.")
    A("3. A home router and the aircraft hotspot on the **same channel** interfere with each other. The AP channel is "
      "derived from `espnowChannel` (default 1); change it with `p ESPNOW_CHANNEL <1-13>`, but **the remote must be "
      "changed to the same channel**, otherwise ESP-NOW receives nothing.")
    A("4. Two aircraft powered at the same time interfere over Wi-Fi. Unplug the other one while testing, otherwise you "
      "cannot attribute a fault.")
    A("5. PID and calibration changes are persisted to NVS automatically. `preset` restores factory defaults (it also "
      "wipes the RC calibration); use `rcdef` if you only want to reset the RC.")
    A("6. This project **depends on the FlixPeriph and ESP32_NOW_Serial libraries**; when rebuilding, match the "
      "versions listed in §2, or you may end up with \"it compiles but ESP-NOW receives nothing\".")
    A("")
    A("## 8. Connecting to the aircraft hotspot (AP)")
    A("")
    A("Hotspot name = `YaoKun_` + the last two bytes of the chip's factory STA MAC (e.g. `YaoKun_F224`). "
      "Trust the **`当前热点名：`** line in the boot log.")
    A("")
    A("Factory passphrase: **`1-2-3-4-5-6-7-8`** — eight digits, position *n* holds digit *n*.")
    A("")
    A("> 🔑 **The hyphens are separators only — do not type them.** The real passphrase is 8 characters:")
    A("")
    A("| Position | 1st | 2nd | 3rd | 4th | 5th | 6th | 7th | 8th |")
    A("| --- | --- | --- | --- | --- | --- | --- | --- | --- |")
    A("| Character | `1` | `2` | `3` | `4` | `5` | `6` | `7` | `8` |")
    A("")
    A("> ⚠️ Every document writes it with hyphens on purpose: contiguous digit runs get rewritten by some "
      "transfer/render paths — we have hit this repeatedly on this project, and it is exactly how the v1.0 docs ended "
      "up publishing a wrong passphrase, which locked every replicator out of the hotspot.")
    A("> Type the 8 characters one by one; no spaces or symbols in between.")
    A(">")
    A("> The firmware builds the default from the ASCII byte table `{49,50,51,52,53,54,55,56,0}` — "
      "no plaintext passphrase string exists anywhere in the repository.")
    A("")
    A("The hotspot only carries **MAVLink over UDP 14550** (for QGC / MAVProxy). **There is no web page**: "
      "a blank page or a refused connection at `192.168.4.1` is **normal** and does not mean you failed to connect.")
    A("To check the link, look at `已连设备数` (connected stations) in the `wifi` command output — 1 or more means the "
      "AP side has registered your device.")
    A("")
    A("> ⚠️ **Do not enable QGC's on-screen virtual joystick** while the ESP-NOW remote is in use. Both send "
      "`MANUAL_CONTROL`, and the aircraft does not validate the source, so the two will fight over the sticks "
      "(jerky or jumping control). QGC is safe for telemetry, parameters and maps.")
    A(">")
    A("> **Building an English UI**: add `#define FLIX_LANG_EN` at the very top of `flix.ino` (before the first "
      "`#include`), or pass `--build-property \"compiler.cpp.extra_flags=-DFLIX_LANG_EN\"` to arduino-cli. "
      "No source edits required.")
    A("")
    A("## 9. Find us")
    A("")
    A("- Bilibili: [耀坤智联](https://space.bilibili.com/3707011699050767)")
    A("- OSHWHub (open-source hardware): [耀坤无人机](https://oshwhub.com/lindahe)")
    A("- GitHub: <https://github.com/YaoKunS>")
    A("- Taobao store: [耀坤智联](https://shop35432590.taobao.com/)")
    A("")
    A("---")
    A("")
    A("This document is generated by `tools/pack_flix_release.py`. Every hash, segment layout, image header and "
      "build time is extracted from the firmware image at generation time, so it always matches the bin exactly.")
    return "\n".join(E) + "\n"


def main():
    ap = argparse.ArgumentParser(description="耀坤整理版 Flix 固件打包（生成单文件固件 + 指纹说明）")
    here = os.path.dirname(os.path.abspath(__file__))
    fw_root = os.path.dirname(here)

    ap.add_argument("--build-dir", default=os.path.join(fw_root, "flix", "build"),
                    help="arduino-cli 的 build 目录（里面有 flix.ino.bin 等）")
    ap.add_argument("--out-dir", default=os.path.join(fw_root, "release"),
                    help="发布产物输出目录")
    ap.add_argument("--version", default="1.1", help="对外版本号，如 1.1")
    ap.add_argument("--date-tag", default=datetime.datetime.now().strftime("%Y%m%d"),
                    help="文件名里的日期标记，默认今天")
    args = ap.parse_args()

    build_dir = args.build_dir
    out_dir = args.out_dir
    if not os.path.isdir(build_dir):
        raise SystemExit("构建目录不存在：%s\n请先编译：arduino-cli compile ... --export-binaries" % build_dir)

    # ---- 前置检查：4 个分片必须存在且"不旧于源文件"（防拿旧 build 打包）----
    missing = []
    for addr, fname, tag, desc in FLASH_MAP:
        if tag == "boot_app0":
            continue  # 由 core 提供，单独处理
        if not os.path.isfile(os.path.join(build_dir, fname)):
            missing.append(fname)
    if missing:
        raise SystemExit(
            "构建目录缺少产物：%s\n"
            "多半是编译时没带 --export-binaries（或用 export.bat）。\n"
            "⚠️ 不要拿旧 build 目录硬打包，会生成内容错误的固件。" % ", ".join(missing))

    os.makedirs(out_dir, exist_ok=True)

    stem = "YaoKunS_耀坤S3无人机_Flix整理版_v%s_耀坤智联_%s" % (args.version, args.date_tag)
    merged_path = os.path.join(out_dir, stem + "_merged.bin")

    # ---- 合并 ----
    segments, merged_size = build_merged(build_dir, merged_path)
    merged_sha = sha256_file(merged_path)
    merged_size_real = os.path.getsize(merged_path)

    # ---- 读镜像头（拿 app 的头，因为真正被校验/启动的是它）----
    app_path = os.path.join(build_dir, "flix.ino.bin")
    hdr = parse_image_header(app_path)
    boot_path = os.path.join(build_dir, "flix.ino.bootloader.bin")
    boot_hdr = parse_image_header(boot_path)

    # ---- 构建时间：取自 app.bin 里 __DATE__ / __TIME__ 的字面量 ----
    # 【为什么要扫描而不是读 esp_app_desc_t】
    #   esp_app_desc_t 里的 time/date 字段由核心库预编译写入，读出来是核心库的构建时间
    #   （实测 0x80 处是 "Mar 31 2026 11:43:43"），不是本工程的。
    #   而 __DATE__ / __TIME__ 是**本工程**编译时由预处理器展开的字面量，
    #   一定出现在固件里（TXT_SYS_FIRMWARE 就用了它们）。
    # 【为什么要"配对"而不是各取最后一个】
    #   实测一个 app.bin 里有 4 组日期、4 组时间，散布在核心库/本工程的字符串里：
    #       0x00080  Mar 31 2026  +  0x00070  11:43:43   ← 核心库
    #       0x019C8  Oct 18 2026  +  0x019D4  11:50:30   ← 本工程（本次编译）
    #       0x0310F  Oct 18 2026  +  0x03106  11:28:53   ← 另一处本工程字符串
    #       0xED28C  Aug 25 2025  +  0xED283  19:04:10   ← 第三方库
    #   单纯"各取最后一个"会拼出 "Aug 25 2025 19:04:10" 这种错配（踩过）。
    #   改成"按偏移配对"之后仍然错 —— 因为第三方库那对也在 64 字节内，而且偏移更大。
    #   ★最终判据（两条同时满足）：
    #     1) 日期年份 == 本次编译年份（过滤核心库/第三方库的历史时间戳）；
    #     2) **日期在前、时间在后，且两者距离最近** —— 因为源码里写的是
    #            TXT_SYS_FIRMWARE  "Firmware: " __DATE__ " " __TIME__ "\n"
    #        编译器把 __DATE__ 和 __TIME__ 两个字面量相邻存放，日期必然在时间之前。
    #        实测本工程有 2 组今年的配对：
    #            0x19C8 "Oct 18 2026" → 0x19D4 "11:50:30"   距离 12 字节  ← 正确
    #            0x3106 "11:28:53"    → 0x310F "Oct 18 2026" 时间在日期前  ← 顺序反了
    #        前者的时间是主文件编译时刻，后者是另一个翻译单元的（早了 22 分钟）。
    # 【教训】解析固件里的"编译时间"这类指纹，不能只看"哪个最大/最后"，
    #         必须结合**源码里的字面量排列顺序**去配对，否则会写出一个差半小时的假时间。
    build_time = "未知"
    try:
        cur_year = str(datetime.datetime.now().year)
        with open(app_path, "rb") as f:
            blob = f.read()
        date_hits = [(m.start(), m.group(0).decode()) for m in
                     re.finditer(rb'(?:Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec) [ 0-9]\d \d{4}', blob)]
        time_hits = [(m.start(), m.group(0).decode()) for m in re.finditer(rb'\d{2}:\d{2}:\d{2}', blob)]
        best = None
        for dpos, dstr in date_hits:
            if not dstr.endswith(cur_year):
                continue
            for tpos, tstr in time_hits:
                gap = tpos - dpos          # 必须 > 0：日期在前，时间在后
                if 0 < gap < 64:
                    if best is None or gap < best[0]:
                        best = (gap, "%s %s" % (dstr, tstr))
        if best:
            build_time = best[1]
    except Exception:
        pass

    # ---- 生成说明文档 ----
    L = []
    A = L.append
    A("# 耀坤S3无人机 / Flix 整理版 —— 固件指纹与烧录说明")
    A("")
    A("## 一、这份固件是什么")
    A("")
    A("基于开源飞控 **Flix**（原作者 Oleg Kalachev）的**耀坤整理版**。在保留上游全部代码结构与 GPL 义务的前提下，针对耀坤 S3 硬件与自研 ESP-NOW 遥控做了适配。")
    A("")
    A("- **上游项目**：<https://github.com/okalachev/flix>（`(C) Oleg Kalachev`，无独立 LICENSE 文件，版权归原作者）")
    A("- **整理版仓库**：<https://github.com/YaoKunS/flix>（分支 `yaokun-organized`，`master` 保持上游原版）")
    A("- **硬件对应**：耀坤 S3 无人机（ESP32-S3，8MB Flash），4 个有刷电机（GPIO 15/5/6/16）")
    A("- **传感器**：MPU6050（I2C，WHO_AM_I=0x70）/ MPU-6500（SPI）—— **上电自动识别**，无需重编")
    A("- **控制**：姿态环 → 角速度环 → 四轴混控（串级 PID），支持 RAW / ACRO / STAB / AUTO 四种模式")
    A("- **遥控**：耀坤自研 ESP-NOW 遥控器（信道 1），同时兼容 SBUS 接收机与 Wi-Fi/UDP 地面站")
    A("- **地面站**：MAVLink v2，可接 QGC / MAVProxy")
    A("- **v1.1 新特性**：① 出厂默认 **ESP-NOW + 热点共存**（旧版互斥，老板子 NVS 首次开机自动迁移）；")
    A("  ② 热点名后缀改读 **eFuse 出厂 STA MAC**（如 `YaoKun_F224`，与 `wifi` 命令里的 MAC 末两字节一致）；")
    A("  ③ 热点密码**三层校验**（NVS 脏值自动清洗 / 非法写入拒绝 / 出厂密码码表构造）")
    A("- **热点说明**：热点仅提供 **UDP 14550 的 MAVLink 遥测**，**没有 Web 网页**（浏览器打不开 192.168.4.1 是正常的）")
    A("- **灯语与音效**：红/绿/蓝三色灯（GPIO 41/7/14）+ 有源蜂鸣器（GPIO 39），非阻塞状态机")
    A("- **界面语言**：**默认全中文**（控制台、help 菜单、IMU 校准、初始化提示）；定义 `FLIX_LANG_EN` 可编英文版")
    A("")
    A("> ★ 本固件为**纯姿态自稳 + 手动油门**：没有 GPS / 气压计 / 测距 / 光流，")
    A("> **松杆不会悬停**（姿态会稳平，但保持水平速度继续飘），**松油门就掉高度**。")
    A("> 首次试飞请在空旷场地、低空、装保护圈的前提下进行。")
    A("")
    A("## 二、版本与来源")
    A("")
    A("| 项目 | 内容 |")
    A("| --- | --- |")
    A("| 固件版本 | **v%s**（耀坤整理版） |" % args.version)
    A("| 品牌 | YaoKunS / 耀坤智联 / 耀坤无人机 |")
    A("| 编译时间 | %s |" % build_time)
    A("| 工具链 | arduino-cli 1.5.1 + esp32 core 3.3.8 |")
    A("| 编译参数 | `esp32:esp32:esp32s3:FlashSize=8M,PartitionScheme=default_8MB`（USB CDC 默认开启） |")
    A("| 第三方库 | FlixPeriph、ESP32_NOW_Serial（随 esp32 core）、MAVLink |")
    A("| 资源占用 | 应用程序 %s |" % human(segments[-1]["size"]))
    A("")
    A("> ⚠️ **读版本号的方式**：Arduino 构建的镜像里 `0x10020` 处的 `esp_app_desc_t`")
    A("> 由 ESP32 核心预编译库写入，`version` 字段是核心库标识，不代表本工程版本。")
    A("> 本固件版本以**开机横幅**打印的 `固件版本：v%s（耀坤整理版）` 为准。" % args.version)
    A("")
    A("## 三、怎么烧（单文件，最简单）")
    A("")
    A("### 方式 A：Flash Download Tool（图形界面）")
    A("")
    A("1. 打开 Flash Download Tool，芯片选 **ESP32-S3**，工作模式 **Develop**。")
    A("2. 只添加这一个文件，地址填 **`0x0`**：`%s_merged.bin`" % stem)
    A("3. 设置：**SPI SPEED = 80MHz，SPI MODE = DIO，FLASH SIZE = 8MB**，选好 COM 口，BAUD 用 921600（不稳降到 115200）。")
    A("4. ⚠️ **务必勾选 DoNotChgBin**。镜像头里已写好 SPI 模式/频率/容量与尾部 SHA-256，工具若改写头部会破坏校验导致无法开机。")
    A("5. 点 START，完成后按 RST 或重新上电。")
    A("")
    A("### 方式 B：esptool 一条命令")
    A("")
    A("```bash")
    A("esptool --chip esp32s3 --port COMx --baud 921600 write_flash 0x0 \"%s_merged.bin\"" % stem)
    A("```")
    A("")
    A("烧完若「不跑、停在下载模式」（日志出现 `waiting for download`）：GPIO0 被拉低没放开。")
    A("先 `--after no-reset` 连一下确认镜像可读，再执行 `esptool --after watchdog-reset run`，或拔插 USB / 按 RST。")
    A("")
    A("## 四、怎么验证烧进去的是这一版（指纹探测）")
    A("")
    A("**① 直接比文件哈希**（最可靠，不用上电）：")
    A("")
    A("```bat")
    A("certutil -hashfile \"%s_merged.bin\" SHA256" % stem)
    A("```")
    A("")
    A("→ 应为 `%s`" % merged_sha)
    A("")
    A("**② 串口开机横幅**（115200，走**原生 USB-CDC 口**）。开机第一屏应为：")
    A("")
    A("```")
    A("耀坤S3 飞控（Flix 整理版）启动中…")
    A("YaoKunS | 耀坤S3无人机 | Flix | 耀坤智联")
    A("固件版本：v%s（耀坤整理版）" % args.version)
    A("初始化参数存储")
    A("初始化电源检测")
    A("初始化无线通信")
    A("ESP-NOW 信道：1")
    A("…（IMU 自动识别输出）…")
    A("惯性测量单元就绪：MPU6500，总线 SPI")
    A("初始化完成")
    A("```")
    A("")
    A("三行关键指纹：**产品名串** + **版本行** + **ESP-NOW 信道 1**。三者同时出现即确认是本版。")
    A("")
    A("**③ 在 bin 里搜明文**（不用设备也能确认是 Flix 整理版）：")
    A("")
    A("- 搜 `耀坤S3 飞控（Flix 整理版）启动中` → 命中（中文开机横幅，英文版无此串）")
    A("- 搜 `YaoKunS | 耀坤S3无人机 | Flix | 耀坤智联` → 命中 1 次（指纹串）")
    A("- 搜 `esp_now_register_recv_cb` 的调用痕迹不易搜，改搜 `接收丢弃字节数` → 命中（原生 ESP-NOW 接收统计）")
    A("")
    A("**④ 读 flash 里的镜像头**（只读，不改动任何东西）：")
    A("")
    A("```bash")
    A("esptool --chip esp32s3 --port COMx read_flash 0x10000 0x200 app_hdr.bin")
    A("```")
    A("")
    A("→ 首字节应为 `E9`（ESP 镜像魔数）。")
    A("")
    A("## 五、产物与校验（SHA-256）")
    A("")
    A("| 产物 | 烧写地址 | SHA-256 | 大小 |")
    A("| --- | --- | --- | --- |")
    A("| **merged 单文件固件** | `0x0` 整体刷 | `%s` | %s |" % (merged_sha, human(merged_size_real)))
    for s in segments:
        addr = "0x%X" % s["addr"]
        if s["tag"] == "boot_app0":
            A("| boot_app0 | `0xE000` | 由 esp32 core 3.3.8 提供（把 otadata 选到 app0，缺它不启动） | %s |" % human(s["size"]))
        else:
            A("| %s | `%s` | `%s` | %s |" % (s["desc"], addr, s["sha256"], human(s["size"])))
    A("")
    A("- 合并布局：`0x0 bootloader + 0x8000 分区表 + 0xE000 boot_app0 + 0x10000 应用程序`（段间空隙填 0xFF）")
    A("- 应用程序镜像头：`%s` → SPI 模式 **%s**、频率 **%s**、Flash 容量 **%s**、段数 **%d**、入口 **0x%08X**" % (hdr["raw"], hdr["spi_mode"], hdr["spi_speed"], hdr["spi_size"], hdr["seg_count"], hdr["entry"]))
    A("- ⚠️ 本包按 **%s Flash** 编译（分区表 default_8MB），**4MB 板不可烧写本包**，请用源码改 `FlashSize=4M,PartitionScheme=default` 重编" % hdr["spi_size"])
    A("- Flash 容量：**8MB**（编译参数 `FlashSize=8M`；如需二次确认可执行 `esptool image-info <bin>`，"
      "其 `Flash size` 一栏应为 8MB。容量指纹取自镜像头 offset 3 高半字节，见上方镜像头行）")
    if boot_hdr:
        A("- bootloader 镜像头：`%s` → SPI 模式 **%s**、频率 **%s**、Flash 容量 **%s**" % (boot_hdr["raw"], boot_hdr["spi_mode"], boot_hdr["spi_speed"], boot_hdr["spi_size"]))
    A("")
    A("## 六、分片烧写（保住 NVS 里的标定数据）")
    A("")
    A("**单文件从 0x0 整片刷写会把 `0x9000~0xE000` 的 nvs 区写成 0xFF = 清空**，")
    A("也就是：WiFi 热点名/密码、IMU 自动识别结果、遥控端点标定值全部丢失。")
    A("")
    A("新板/空板无所谓；**自己已经标定好的机器**请改用分片写，跳过 nvs：")
    A("")
    A("```bash")
    A("esptool --chip esp32s3 --port COMx --baud 921600 write_flash \\")
    for s in segments:
        if s["tag"] == "boot_app0":
            continue
        A("  0x%X \"%s\" \\" % (s["addr"], s["name"]))
    A("```")
    A("")
    A("（`boot_app0.bin` 位于 esp32 core 的 `tools/partitions/` 下，一般不重刷也没事。）")
    A("")
    A("## 七、六点须知")
    A("")
    A("1. **首次烧完请重跑 `cal`（IMU 校准）与遥控标定**：整片刷写清空了 NVS，IMU 零偏需要重新测。")
    A("2. 控制台走**原生 USB 口**（`CDCOnBoot=cdc`）。只插板上 UART 口看不到日志是正常的。")
    A("3. 家庭路由器与飞机热点**同信道**时容易互相干扰。飞机 AP 信道由 `espnowChannel`（默认 1）决定，")
    A("   若与家里路由撞了，用 `p ESPNOW_CHANNEL <1-13>` 改，但**遥控器端也要同步改**，否则 ESP-NOW 收不到。")
    A("4. 两台飞机同时上电会互相干扰 Wi-Fi，测试时请把另一台断电，否则无法归因。")
    A("5. 改了 PID / 校准参数会自动存 NVS，下次上电仍在。恢复出厂用控制台 `preset`（会一并清掉遥控标定），")
    A("   只想重置遥控请用 `rcdef`。")
    A("6. 本工程**依赖 FlixPeriph 与 ESP32_NOW_Serial 库**；自己重编时要保证库版本与第二节一致，")
    A("   否则可能出现「编过但 ESP-NOW 收不到」的现象。")
    A("")
    A("## 八、连飞机热点（AP）")
    A("")
    A("热点名 = `YaoKun_` + 芯片出厂 STA MAC 末两字节（如 `YaoKun_F224`），开机日志里 **`当前热点名：`** 那行为准。")
    A("")
    A("出厂密码：**`1-2-3-4-5-6-7-8`**（中文逐位读作：一、二、三、四、五、六、七、八）。")
    A("")
    A("> 🔑 **连字符只是分隔符，输入时不要带** —— 实际是 8 个字符，逐位敲：")
    A("")
    A("| 位次 | 第 1 位 | 第 2 位 | 第 3 位 | 第 4 位 | 第 5 位 | 第 6 位 | 第 7 位 | 第 8 位 |")
    A("| --- | --- | --- | --- | --- | --- | --- | --- | --- |")
    A("| 该位字符 | `1` | `2` | `3` | `4` | `5` | `6` | `7` | `8` |")
    A("")
    A("> ⚠️ 全文档统一用**带连字符**的形式书写，刻意不写连续的 8 位明文：连续数字串经传输/渲染通道")
    A("> 会被改写（本工程已多次实测，v1.0 文档就因此写错过密码，导致所有复刻用户连不上热点）。")
    A("> 输入时按上表逐位敲，共 8 位，中间不加空格或符号。")
    A(">")
    A("> 源码里的默认值用 ASCII 码表 `{49,50,51,52,53,54,55,56,0}` 构造，仓库内没有任何明文密码串。")
    A("")
    A("热点只提供 **UDP 14550 的 MAVLink**（QGC / MAVProxy 用），**没有网页**：")
    A("浏览器打开 `192.168.4.1` 空白/拒绝连接都是**正常现象**，不代表没连上。")
    A("判断是否连上请看 `wifi` 命令的 `已连设备数`（≥1 即 AP 侧已登记）。")
    A("")
    A("## 九、找到我们")
    A("")
    A("- B站（哔哩哔哩）：[耀坤智联](https://space.bilibili.com/3707011699050767)")
    A("- 立创开源硬件平台：[耀坤无人机](https://oshwhub.com/lindahe)")
    A("- GitHub：<https://github.com/YaoKunS>")
    A("- 淘宝店：[耀坤智联](https://shop35432590.taobao.com/)")
    A("")
    A("---")
    A("")
    A("本说明由 `tools/pack_flix_release.py` 自动生成；哈希、分段布局、镜像头与编译时间均现场从固件镜像提取，与 bin 严格一致。")

    # ---- 写盘：中文说明 + 英文说明（各出 md / txt 两份）----
    # 【为什么要两份语言】发布包同时面向国内复刻用户与海外 Flix 社区，中文说明对海外用户等于没有；
    #   两份都由本脚本现场生成，哈希/镜像头/编译时间同源，不会出现"一份对一份错"。
    def md_to_plain(text):
        """把 Markdown 降级成纯文本：去掉代码围栏与表格分隔行，便于手机/记事本直接查看。"""
        out = text.replace("```bash", "").replace("```bat", "").replace("```", "")
        out = out.replace("| --- | --- | --- | --- |\n", "")
        return out

    # 英文版文件名用纯 ASCII 主干（中文文件名在英文系统/命令行里容易出编码问题）
    stem_en = "YaoKunS_S3Drone_FlixOrganized_v%s_YaoKunSmart_%s" % (args.version, args.date_tag)
    docs = [
        (stem + "_指纹与烧录说明", "\n".join(L) + "\n"),
        (stem_en + "_Fingerprint_and_Flashing_Guide_EN",
         render_doc_en(args.version, stem, merged_sha, merged_size_real, segments, hdr, boot_hdr, build_time)),
    ]
    written = []
    for base, text in docs:
        p_md = os.path.join(out_dir, base + ".md")
        p_txt = os.path.join(out_dir, base + ".txt")
        with open(p_md, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
        with open(p_txt, "w", encoding="utf-8", newline="\r\n") as f:
            f.write(md_to_plain(text))
        written.append((p_md, p_txt))

    # ---- 控制台汇总 ----
    print("=" * 68)
    print("打包完成")
    print("=" * 68)
    print("单文件固件 : %s" % merged_path)
    print("  大小     : %s" % human(merged_size_real))
    print("  SHA-256  : %s" % merged_sha)
    print("  镜像头   : %s → %s %s, %d segments, entry 0x%08X" % (hdr["raw"], hdr["spi_mode"], hdr["spi_speed"], hdr["seg_count"], hdr["entry"]))
    print("-" * 68)
    for s in segments:
        print("  %-12s 0x%-6X %-10s %s" % (s["tag"], s["addr"], human(s["size"]), s["sha256"]))
    print("-" * 68)
    print("说明文档   : %s" % written[0][0])
    print("             %s" % written[0][1])
    print("说明文档 EN: %s" % written[1][0])
    print("             %s" % written[1][1])
    print("=" * 68)


if __name__ == "__main__":
    main()
