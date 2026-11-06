#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
微辣火龙果 2.0（ESP32 经典片）发布说明生成器
================================================================================

【这个脚本解决什么问题】
  发布包里的《指纹与烧录说明》必须与 bin **现场一致**：哈希、大小、编译时间
  任何一项写死，下次重出固件就会漂（用户按文档校验必然失败）。
  所以本脚本不存任何硬编码数字，全部从整片 bin 里现读现算。

【为什么火龙果不复用 pack_flix_release.py】
  那个脚本从合并、脱敏到文案都是围绕「耀坤 S3（ESP32-S3）」写的：
    · bootloader 偏移 0x0（S3/C3/C6 规则）
    · 8MB Flash + default_8MB 分区表
    · 16 脚 / S3 引脚表
  火龙果是 **ESP32 经典片**，bootloader 偏移 **0x1000**、Flash 4MB、21 脚，
  而且它没有「S3 零变化」这类约束，文案完全不同 —— 强行塞进一个脚本只会
  让两边的分支互相污染。因此单独一个小工具，只负责「读整片 → 渲染说明」。

【输入】必须是 **已脱敏、已修好 trailer、已合并成整片** 的 4MB bin。
  本脚本不做脱敏、不改字节，只读。
  原因：改镜像字节需要同时重算 XOR 校验字节与 appended SHA-256
  （见 pack_flix_release.py 的 fix_app_image_trailer），把这两件事拆开更不容易出错。

【用法】
  python tools/gen_hlg_note.py --bin ../release固件v1.1/xxx_整片4MB.bin \
      --out-dir ../release固件v1.1 --version 1.1 --date-tag 20261106

【产出】中文 md/txt + 英文 md/txt，共 4 个文件。
"""

import argparse
import datetime
import hashlib
import os
import re
import struct

# ---------------------------------------------------------------------------
# 硬件常量（火龙果 2.0 / ESP32 DevKit V1 30 针）
# 【来源】实测 + 作者开源资料（oshwhub.com/malagis/esp32-mini-plane）
# 【重要】电机顺序 = 丝印顺序的「右后 / 右前 / 左后 / 左前」，
#   与常见 QuadX 编号（1 右后 / 2 右前 / 3 左后 / 4 左前）一致，混控不做交叉。
# ---------------------------------------------------------------------------
BOARD_NAME_CN = "微辣火龙果 2.0"
BOARD_NAME_EN = "MildlySpicyDragon 2.0"
CHIP = "ESP32（经典 Xtensa LX6，双核 240MHz）"
FLASH_MB = 4

MOTOR_PINS = "26 / 27 / 33 / 32（依次 RL / RR / FR / FL）"
IMU_I2C = "SDA 21 / SCL 22（I2C0）"
IMU_SPI = "MOSI 23 / MISO 19 / SCLK 18 / CS 5（SPI）"
FLOW_PINS = "13 / 14 / 12 / 15（光流预留）"
LASER_PINS = "SDA 16 / SCL 17（I2C1，激光测距预留）"
BUZZER_PIN = "25"
ADC_PIN = "35（电池电压检测，ADC1_CH7）"
LED_PIN = "2"
SBUS_PIN = "4（SBus 接收机）"

APP_OFFSET = 0x10000     # app 分区起始（default 分区表固定值，实测确认）
BOOT_OFFSET = 0x1000     # 🔴 ESP32 经典片 bootloader 在 0x1000（S3/C3/C6 才是 0x0）


def sha256_file(path):
    """返回文件的 SHA-256 十六进制串（大写，与 certutil 输出一致便于人工比对）。"""
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest().upper()


def parse_app_header(buf, off):
    """
    解析 ESP app 镜像头，返回段数与「镜像数据结束位置」（不含 trailer）。

    ESP 镜像头布局（app 一定有 16 字节扩展头，bootloader 没有）：
      0x00 magic 0xE9 | 0x01 段数 | 0x02 SPI 模式 | 0x03 SPI 速度/容量
      0x04 入口地址(4B) | 0x08 扩展头(16B：版本/项目名/时间/日期…)
      之后每段：4B 加载地址 + 4B 长度 + 数据。
    """
    if buf[off] != 0xE9:
        raise SystemExit("偏移 0x%X 处不是 ESP 镜像（首字节 0x%02X，应为 0xE9）" % (off, buf[off]))
    nseg = buf[off + 1]
    pos = off + 8 + 16          # 8B 公共头 + 16B 扩展头
    pos -= off                  # 转成相对偏移，方便调用方计算
    p = off + pos
    for _ in range(nseg):
        _load, ln = struct.unpack_from("<II", buf, p)
        p += 8 + ln
    return nseg, p


def extract_build_time(buf, off):
    """
    从 app 镜像里提取本工程的编译时间（__DATE__ " " __TIME__）。

    【为什么要配对而不是取最后一个】
      app 里有多组日期/时间字面量：核心库预编译的、第三方库的、本工程的。
      例如火龙果这颗 app 实测有 4 个 HH:MM:SS，单纯取最大值会拿到别的时间。
      判据（与 pack_flix_release.py 一致）：
        1) 年份 == 当前年（过滤历史时间戳）；
        2) **日期在前、时间在后且距离最近**（源码里写的是 __DATE__ " " __TIME__，
           编译器把两个字面量相邻存放）。
    """
    cur_year = str(datetime.datetime.now().year).encode()
    dates = [(m.start(), m.group(0)) for m in
             re.finditer(rb'(?:Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec) [ 0-9]\d ' + cur_year, buf[off:])]
    times = [(m.start(), m.group(0)) for m in
             re.finditer(rb'\d{2}:\d{2}:\d{2}', buf[off:])]
    best = None
    for dp, ds in dates:
        for tp, ts in times:
            gap = tp - dp
            if 0 < gap < 64 and (best is None or gap < best[0]):
                best = (gap, (ds + b" " + ts).decode())
    return best[1] if best else "未知"


def md_to_plain(text):
    """Markdown → 纯文本：去掉代码围栏与表格分隔行，方便手机/记事本查看。"""
    out = text.replace("```bash", "").replace("```bat", "").replace("```", "")
    out = re.sub(r"^\| (?:--- \| )+--- \|$", "", out, flags=re.M)
    return out


def render_cn(v, stem, full_sha, full_size, app_size, nseg, build_time, date_tag):
    """渲染中文说明。所有 %s 都由调用方现场测量后传入。"""
    L = []
    A = L.append
    A("# 微辣火龙果 2.0 / Flix 整理版 —— 固件指纹与烧录说明")
    A("")
    A("## 一、这份固件是什么")
    A("")
    A("基于开源飞控 **Flix**（原作者 Oleg Kalachev）的**耀坤整理版**，针对「微辣火龙果 2.0」"
      "这块 ESP32 迷你无人机做了板型适配。同一套源码通过**编译期板型宏**（`FLIX_BOARD=2`）"
      "同时支持耀坤 S3 与火龙果 2.0 两块 PCB。")
    A("")
    A("- **上游项目**：<https://github.com/okalachev/flix>（`(C) Oleg Kalachev`，无独立 LICENSE 文件，版权归原作者）")
    A("- **整理版仓库**：<https://github.com/YaoKunS/flix>（分支 `yaokun-organized`）")
    A("- **硬件设计**：微辣火龙果（malagis / MildlySpicyDragon），"
      "立创开源硬件平台 <https://oshwhub.com/malagis/esp32-mini-plane>")
    A("- **固件归属**：**火龙果版固件（板型适配与维护）由耀坤智联（YaoKunSmart）负责**。"
      "硬件版权归火龙果作者，内核版权归 Oleg Kalachev，两套品牌互相独立。")
    A("")
    A("> ★ 本固件为**纯姿态自稳 + 手动油门**：没有 GPS / 气压计 / 测距 / 光流，")
    A("> **松杆不会悬停**（姿态会稳平，但保持水平速度继续飘），**松油门就掉高度**。")
    A("> 首次试飞请在空旷场地、低空、装保护圈的前提下进行。")
    A("")
    A("## 二、版本与来源")
    A("")
    A("| 项目 | 内容 |")
    A("| --- | --- |")
    A("| 固件版本 | **v%s**（耀坤整理版 · 微辣火龙果 2.0） |" % v)
    A("| 编译时间 | %s |" % build_time)
    A("| 芯片 | %s |" % CHIP)
    A("| Flash | %d MB（整片固件已补齐到 %d MB） |" % (FLASH_MB, FLASH_MB))
    A("| 编译参数 | `esp32:esp32:esp32:FlashSize=4M,PartitionScheme=default` + `-DFLIX_BOARD=2` |")
    A("| 应用程序大小 | %s B（0x%X） |" % (format(app_size, ","), app_size))
    A("| 整片大小 | %s B（0x%X） |" % (format(full_size, ","), full_size))
    A("| 整片 SHA-256 | `%s` |" % full_sha)
    A("")
    A("> ⚠️ **读版本号的方式**：Arduino 构建的镜像里 `0x10020` 处的 `esp_app_desc_t`")
    A("> 由 ESP32 核心预编译库写入，`version` 字段是核心库标识，不代表本工程版本。")
    A("> 本固件版本以**开机横幅**打印的 `固件版本：v%s（耀坤整理版 · 微辣火龙果 2.0）` 为准。" % v)
    A("")
    A("## 三、硬件引脚")
    A("")
    A("| 功能 | 引脚 |")
    A("| --- | --- |")
    A("| 电机（PWM） | %s |" % MOTOR_PINS)
    A("| IMU（I2C，MPU6050 等） | %s |" % IMU_I2C)
    A("| IMU（SPI，MPU6500 等） | %s |" % IMU_SPI)
    A("| 蜂鸣器 | %s |" % BUZZER_PIN)
    A("| 状态 LED | %s |" % LED_PIN)
    A("| 电池电压 | %s |" % ADC_PIN)
    A("| SBus 接收机 | %s |" % SBUS_PIN)
    A("| 光流（预留） | %s |" % FLOW_PINS)
    A("| 激光测距（预留） | %s |" % LASER_PINS)
    A("")
    A("> 引脚在源码里由 `Firmware/flix/board.h` 的 `BOARD_HLG_V2` 分支统一定义。")
    A("> **不要照 S3 的引脚表接线**——两块板电机脚完全不同，接错上电即乱序起转。")
    A("")
    A("## 四、怎么烧（单文件，最简单）")
    A("")
    A("### 方式 A：Flash Download Tool（图形界面，推荐给不熟命令行的同学）")
    A("")
    A("1. 打开 Flash Download Tool，芯片选 **ESP32**（⚠️ 不是 ESP32-S3，这是经典片）。")
    A("2. 工作模式 **Develop**，只添加这一个文件，地址填 **`0x0`**：")
    A("")
    A("   `%s_整片%dMB.bin`" % (stem, FLASH_MB))
    A("")
    A("3. 设置：**SPI SPEED = 40MHz，SPI MODE = DIO，FLASH SIZE = %dMB**，选好 COM 口，BAUD 用 460800。" % FLASH_MB)
    A("4. ⚠️ **务必勾选 DoNotChgBin**。镜像头里已写好 SPI 模式/频率/容量与尾部 SHA-256，"
      "工具若改写头部会破坏校验导致无法开机。")
    A("5. 点 START，完成后按 RST 或重新上电。")
    A("")
    A("### 方式 B：esptool 一条命令")
    A("")
    A("```bash")
    A('esptool --chip esp32 --port COMx --baud 460800 write_flash 0x0 "%s_整片%dMB.bin"' % (stem, FLASH_MB))
    A("```")
    A("")
    A("### 🔴 如果只单独烧某一段（进阶，容易踩坑）")
    A("")
    A("| 段 | 地址 | 说明 |")
    A("| --- | --- | --- |")
    A("| bootloader | **`0x1000`** | 🔴 ESP32 经典片是 **0x1000**，不是 0x0！ |")
    A("| 分区表 | `0x8000` | default 4MB 方案 |")
    A("| OTA 数据初值 | `0xE000` | boot_app0.bin |")
    A("| 应用程序 | `0x10000` | 只改固件时只写这里，NVS 参数全保留 |")
    A("")
    A("> 🔴 **最容易踩的坑**：把 S3 的 `0x0` 照抄过来烧 bootloader，"
      "ESP32 经典片的 ROM 会在 0x1000 找不到 `0xE9` 头，"
      "表现是「烧录显示 verified 成功，但板子不启动、串口 0 字节、停在 waiting for download」。")
    A("")
    A("## 五、怎么验证烧进去的是这一版（指纹探测）")
    A("")
    A("1. 串口工具连 COM 口，**115200 8N1**，按一下 RST 或重新上电。")
    A("2. 首屏应出现下面这段横幅（关键行已标出）：")
    A("")
    A("```")
    A("==============================================================")
    A("  微辣火龙果 2.0 飞控（基于Flix耀坤版） MildlySpicyDragon")
    A("  2025 星火计划获奖项目 · ESP32 迷你无人机 · ESP32 DevKit V1")
    A("  立创硬件开源：https://oshwhub.com/malagis/esp32-mini-plane")
    A("  B站微辣火龙果：https://space.bilibili.com/544479100")
    A("--------------------------------------------------------------")
    A("  固件开源：https://github.com/YaoKunS/flix")
    A("--------------------------------------------------------------")
    A("  内核 (C) Oleg Kalachev   https://github.com/okalachev/flix")
    A("  硬件设计：微辣火龙果（malagis）· 立创开源硬件平台")
    A("  本固件由耀坤智联二次开发（目录整理 / 中文界面 / 硬件适配）")
    A("  淘宝：耀坤智联  https://shop35432590.taobao.com/")
    A("==============================================================")
    A("```")
    A("")
    A("3. 横幅之后应打印 `固件版本：v%s（耀坤整理版 · 微辣火龙果 2.0）`。" % v)
    A("4. 想校验文件完整性（Windows）：")
    A("")
    A("```bat")
    A('certutil -hashfile "%s_整片%dMB.bin" SHA256' % (stem, FLASH_MB))
    A("```")
    A("")
    A("   输出应与上表 `整片 SHA-256` 完全一致（本工具算的是大写，certutil 输出通常带空格，忽略大小写与空格比对即可）。")
    A("")
    A("## 六、连飞机热点（AP）")
    A("")
    A("热点名 = `YaoKun_` + 芯片出厂 STA MAC 末两字节（每块板不同，如 `YaoKun_2DD4`），")
    A("**以开机日志里 `当前热点名：` 那行为准**。")
    A("")
    A("出厂密码：**`1-2-3-4-5-6-7-8`**（中文逐位读作：一、二、三、四、五、六、七、八）。")
    A("")
    A("> 🔑 **连字符只是分隔符，输入时不要带** —— 实际是 8 个字符，逐位敲：")
    A("")
    A("| 位次 | 第 1 位 | 第 2 位 | 第 3 位 | 第 4 位 | 第 5 位 | 第 6 位 | 第 7 位 | 第 8 位 |")
    A("| --- | --- | --- | --- | --- | --- | --- | --- | --- |")
    A("| 该位字符 | `1` | `2` | `3` | `4` | `5` | `6` | `7` | `8` |")
    A("")
    A("> ⚠️ 全文档统一用**带连字符**的形式书写，刻意不写连续的 8 位明文：")
    A("> 连续数字串经传输/渲染通道会被改写（本工程已多次实测）。输入时按上表逐位敲。")
    A("")
    A("热点只提供 **UDP 14550 的 MAVLink**（QGC / MAVProxy 用），**没有网页**：")
    A("浏览器打开 `192.168.4.1` 空白/拒绝连接都是**正常现象**。")
    A("判断是否连上请看 `wifi` 命令的 `已连设备数`（≥1 即 AP 侧已登记）。")
    A("")
    A("## 七、遥控（ESP-NOW / SBus）")
    A("")
    A("- **ESP-NOW**：耀坤自研遥控器，信道固定 **1**。遥控器与飞机必须同一信道。")
    A("- 🔴 中文版遥控器**默认未使能**：连上后要按一下遥控器「左按钮」才发杆量。")
    A("  **飞机每次重启都要重连并重按**（不是故障，是防误触设计）。")
    A("- **SBus**：接 `%s`，串口自动识别；首次使用请跑 `cr` 重新标定。" % SBUS_PIN)
    A("- 🔴 **不同遥控器的标定不能混用**（满杆值差很多，混用会打杆异常）。换控必重跑 `cr`。")
    A("")
    A("## 八、找到我们")
    A("")
    A("- B站（哔哩哔哩）：[耀坤智联](https://space.bilibili.com/3707011699050767)")
    A("- 立创开源硬件平台：[耀坤无人机](https://oshwhub.com/lindahe)")
    A("- GitHub：<https://github.com/YaoKunS>")
    A("- 淘宝店：[耀坤智联](https://shop35432590.taobao.com/)")
    A("- 硬件原作者：微辣火龙果（malagis）· <https://oshwhub.com/malagis/esp32-mini-plane>")
    A("")
    A("---")
    A("")
    A("本说明由 `Firmware/tools/gen_hlg_note.py` 自动生成（%s）；" % date_tag)
    A("哈希、大小、分段布局与编译时间均现场从整片固件提取，与 bin 严格一致。")
    return "\n".join(L) + "\n"


def render_en(v, stem, full_sha, full_size, app_size, build_time, date_tag):
    """渲染英文说明（面向海外 Flix 社区）。内容与中文版同源，只换语言。"""
    L = []
    A = L.append
    A("# %s / Flix Organized Edition — Firmware Fingerprint & Flashing Guide" % BOARD_NAME_EN)
    A("")
    A("## 1. What this firmware is")
    A("")
    A("An **organized edition of the open-source flight controller Flix** "
      "(original author Oleg Kalachev), adapted for the **%s** ESP32 mini drone." % BOARD_NAME_EN)
    A("The same source tree supports both the YaoKun S3 board and the MildlySpicyDragon 2.0 "
      "through a **compile-time board macro** (`FLIX_BOARD=2`).")
    A("")
    A("- Upstream: <https://github.com/okalachev/flix> — `(C) Oleg Kalachev` (no standalone LICENSE file; copyright stays with the author)")
    A("- Organized-edition repo: <https://github.com/YaoKunS/flix> (branch `yaokun-organized`)")
    A("- Hardware design: MildlySpicyDragon (malagis) — <https://oshwhub.com/malagis/esp32-mini-plane>")
    A("- **Firmware maintenance:** the %s port (board adaptation and maintenance) is "
      "**maintained by YaoKunSmart**. Hardware copyright belongs to the dragon-fruit author; "
      "kernel copyright belongs to Oleg Kalachev. The two brands are independent." % BOARD_NAME_EN)
    A("")
    A("> ★ Flight model: **attitude self-leveling + manual throttle** only. "
      "No GPS / barometer / rangefinder / optical flow. Centering the sticks does **not** hover; "
      "releasing throttle **will** drop altitude.")
    A("")
    A("## 2. Version & build")
    A("")
    A("| Item | Value |")
    A("| --- | --- |")
    A("| Firmware version | **v%s** (organized edition, %s) |" % (v, BOARD_NAME_EN))
    A("| Build time | %s |" % build_time)
    A("| Chip | %s |" % CHIP)
    A("| Flash | %d MB (full image padded to %d MB) |" % (FLASH_MB, FLASH_MB))
    A("| Build flags | `esp32:esp32:esp32:FlashSize=4M,PartitionScheme=default` + `-DFLIX_BOARD=2` |")
    A("| Application size | %s B (0x%X) |" % (format(app_size, ","), app_size))
    A("| Full image size | %s B (0x%X) |" % (format(full_size, ","), full_size))
    A("| Full image SHA-256 | `%s` |" % full_sha)
    A("")
    A("> The `version` field inside `esp_app_desc_t` (at `0x10020`) is written by the "
      "precompiled ESP32 core and is **not** this project's version. "
      "Trust the boot banner: `Firmware version: v%s (YaoKun organized edition, %s)`." % (v, BOARD_NAME_EN))
    A("")
    A("## 3. Pinout")
    A("")
    A("| Function | Pin |")
    A("| --- | --- |")
    A("| Motors (PWM) | %s |" % MOTOR_PINS)
    A("| IMU (I2C) | %s |" % IMU_I2C)
    A("| IMU (SPI) | %s |" % IMU_SPI)
    A("| Buzzer | %s |" % BUZZER_PIN)
    A("| Status LED | %s |" % LED_PIN)
    A("| Battery voltage | %s |" % ADC_PIN)
    A("| SBus receiver | %s |" % SBUS_PIN)
    A("| Optical flow (reserved) | %s |" % FLOW_PINS)
    A("| Laser rangefinder (reserved) | %s |" % LASER_PINS)
    A("")
    A("> Pins live in `Firmware/flix/board.h`, branch `BOARD_HLG_V2`. "
      "**Do not reuse the S3 pinout** — the motor pins are completely different.")
    A("")
    A("## 4. Flashing (single file)")
    A("")
    A("### Option A: Flash Download Tool (GUI)")
    A("")
    A("1. Select chip **ESP32** (⚠️ not ESP32-S3 — this is the classic ESP32).")
    A("2. Develop mode, add only this file at address **`0x0`**: `%s_4MB.bin`" % stem)
    A("3. SPI SPEED **40MHz**, SPI MODE **DIO**, FLASH SIZE **%dMB**, baud 460800." % FLASH_MB)
    A("4. ⚠️ **Tick DoNotChgBin.** The image header carries SPI mode/speed/size plus the "
      "appended SHA-256; rewriting the header breaks boot validation.")
    A("")
    A("### Option B: esptool")
    A("")
    A("```bash")
    A('esptool --chip esp32 --port COMx --baud 460800 write_flash 0x0 "%s_4MB.bin"' % stem)
    A("```")
    A("")
    A("### 🔴 Segment addresses (advanced)")
    A("")
    A("| Segment | Address | Note |")
    A("| --- | --- | --- |")
    A("| bootloader | **`0x1000`** | 🔴 Classic ESP32 uses **0x1000**, NOT 0x0 |")
    A("| partition table | `0x8000` | default 4MB layout |")
    A("| OTA data init | `0xE000` | boot_app0.bin |")
    A("| application | `0x10000` | app-only update keeps all NVS settings |")
    A("")
    A("> 🔴 Most common mistake: flashing the bootloader at `0x0` (the ESP32-S3 rule). "
      "The classic ESP32 ROM looks for the `0xE9` magic at `0x1000`; you get a flash that "
      "reports *verified* but never boots (0 bytes on serial, stuck at `waiting for download`).")
    A("")
    A("## 5. Verify what you flashed")
    A("")
    A("1. Open the port at **115200 8N1** and press RST / power-cycle.")
    A("2. The boot banner should read:")
    A("")
    A("```")
    A("==============================================================")
    A("  MildlySpicyDragon 2.0 Flight Controller (Flix YaoKun Edition)")
    A("  2025 Spark Program Award Project - ESP32 Mini Drone - ESP32 DevKit V1")
    A("  Open hardware: https://oshwhub.com/malagis/esp32-mini-plane")
    A("  Bilibili: https://space.bilibili.com/544479100")
    A("--------------------------------------------------------------")
    A("  Firmware: https://github.com/YaoKunS/flix")
    A("--------------------------------------------------------------")
    A("  Kernel (C) Oleg Kalachev  https://github.com/okalachev/flix")
    A("  Hardware design: MildlySpicyDragon (malagis)")
    A("  Firmware port maintained by YaoKunSmart")
    A("  Taobao (YaoKunSmart): https://shop35432590.taobao.com/")
    A("==============================================================")
    A("```")
    A("")
    A("3. Then `Firmware version: v%s (YaoKun organized edition, %s)`." % (v, BOARD_NAME_EN))
    A("4. SHA-256 check (Windows): `certutil -hashfile \"%s_4MB.bin\" SHA256`" % stem)
    A("")
    A("## 6. Wi-Fi hotspot")
    A("")
    A("SSID = `YaoKun_` + last two bytes of the factory STA MAC (e.g. `YaoKun_2DD4`). "
      "Always trust the `当前热点名：` line in the boot log.")
    A("")
    A("Factory password: **`1-2-3-4-5-6-7-8`** — the hyphens are separators only, "
      "type the eight characters one by one (digits one through eight).")
    A("")
    A("The hotspot serves **MAVLink on UDP 14550 only** — there is **no web page**. "
      "A blank `192.168.4.1` is expected; check `已连设备数` from the `wifi` command instead.")
    A("")
    A("## 7. Radio control")
    A("")
    A("- **ESP-NOW**: YaoKun RC, fixed channel **1**.")
    A("- 🔴 The Chinese RC ships **disabled** — press the left button once after connecting. "
      "This must be repeated after every aircraft reboot (anti-mistouch design, not a bug).")
    A("- **SBus**: wire to GPIO `%s`. Run `cr` to calibrate on first use." % SBUS_PIN)
    A("- 🔴 Calibrations are **not interchangeable** between different transmitters.")
    A("")
    A("## 8. Links")
    A("")
    A("- Bilibili: <https://space.bilibili.com/3707011699050767>")
    A("- OSHWHub: <https://oshwhub.com/lindahe>")
    A("- GitHub: <https://github.com/YaoKunS>")
    A("- Taobao: <https://shop35432590.taobao.com/>")
    A("- Hardware author: MildlySpicyDragon (malagis) — <https://oshwhub.com/malagis/esp32-mini-plane>")
    A("")
    A("---")
    A("")
    A("Generated by `Firmware/tools/gen_hlg_note.py` (%s). "
      "Hashes, sizes, layout and build time are read from the binary itself." % date_tag)
    return "\n".join(L) + "\n"


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    fw_root = os.path.dirname(here)
    ap = argparse.ArgumentParser(description="微辣火龙果 2.0 固件说明生成器（只读 bin，不改字节）")
    ap.add_argument("--bin", required=True, help="已脱敏/已修 trailer 的整片 4MB bin 路径")
    ap.add_argument("--out-dir", default=fw_root, help="说明文档输出目录")
    ap.add_argument("--version", default="1.1")
    ap.add_argument("--date-tag", default=datetime.datetime.now().strftime("%Y%m%d"))
    args = ap.parse_args()

    with open(args.bin, "rb") as f:
        buf = f.read()

    # ---- 前置校验：必须是 4MB 整片，且 app 在 0x10000 处有 0xE9 头 ----
    #   不做这层校验的话，误传一个 app-only bin 也能"生成文档"，但描述全是错的。
    if len(buf) != FLASH_MB * 1024 * 1024:
        raise SystemExit("整片大小应为 %d MB，实际 %.2f MB —— 请确认传的是「整片」而不是 app" %
                         (FLASH_MB, len(buf) / 1048576))
    if buf[BOOT_OFFSET] != 0xE9:
        raise SystemExit("0x%X 处不是 bootloader（首字节 0x%02X）—— ESP32 经典片应在 0x1000" %
                         (BOOT_OFFSET, buf[BOOT_OFFSET]))
    if buf[APP_OFFSET] != 0xE9:
        raise SystemExit("0x%X 处不是 app 镜像，分区布局异常" % APP_OFFSET)

    nseg, app_end = parse_app_header(buf, APP_OFFSET)
    # app 真实长度 = 镜像数据 + trailer（校验字节 1 + SHA 32），再补零到 16
    # 🔴 这里必须减去 APP_OFFSET：cs_off 是整片里的**绝对**偏移，
    #   直接用会多出 0x10000（踩过：文档里 app 大小写成 1,130,384 而实际 1,064,848）。
    trailer_pos = app_end
    cs_off = trailer_pos + ((15 - trailer_pos % 16) % 16)
    app_size = ((cs_off + 1 + 32 - APP_OFFSET) + 15) // 16 * 16

    full_sha = sha256_file(args.bin)
    full_size = len(buf)
    build_time = extract_build_time(buf, APP_OFFSET)

    stem = "MildlySpicyDragon_火龙果2.0_Flix整理版_v%s_%s" % (args.version, args.date_tag)
    os.makedirs(args.out_dir, exist_ok=True)

    docs = [
        (stem + "_指纹与烧录说明",
         render_cn(args.version, stem, full_sha, full_size, app_size, nseg, build_time, args.date_tag)),
        ("MildlySpicyDragon_Drone_FlixOrganized_v%s_YaoKunSmart_%s_Fingerprint_and_Flashing_Guide_EN"
         % (args.version, args.date_tag),
         render_en(args.version, stem, full_sha, full_size, app_size, build_time, args.date_tag)),
    ]
    for base, text in docs:
        p_md = os.path.join(args.out_dir, base + ".md")
        p_txt = os.path.join(args.out_dir, base + ".txt")
        with open(p_md, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
        with open(p_txt, "w", encoding="utf-8", newline="\r\n") as f:
            f.write(md_to_plain(text))
        print("生成:", p_md)
        print("      ", p_txt)

    print("-" * 60)
    print("整片大小     : %s B" % format(full_size, ","))
    print("app 大小     : %s B (0x%X)" % (format(app_size, ","), app_size))
    print("app 段数     : %d" % nseg)
    print("编译时间     : %s" % build_time)
    print("整片 SHA-256 : %s" % full_sha)


if __name__ == "__main__":
    main()
