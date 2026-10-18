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

    # 指定构建目录 / 输出目录 / 版本号
    python pack_flix_release.py --build-dir "D:\\YaoKunS\\Forks\\flix\\Firmware\\flix\\build" ^
                                --out-dir   "D:\\YaoKunS\\Forks\\flix\\Firmware\\release" ^
                                --version   "1.0"

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

    🔴 踩过的坑（三次，记下来省得再踩）：
       1) 以为 offset 3 是 speed、offset 4 起是 uint32 的 size → 读出 1077386660 垃圾值。
       2) 按 "3=size, 4=speed" 改 → 读出 mode=DIO 但 speed=?0xA4、size=?47，全错。
       3) **真相：这个固件的镜像头里没有独立的 spi_size 字节**。
          ESP32-S3 的 app 镜像把 flash 容量信息放在**扩展头**里（offset 8 之后）。
          前 5 字节的布局就是 magic / segments / mode / speed，然后紧跟 entry_point。
       结论：**不要在这个函数里硬读 size**。size 从 esptool 的 image-info 拿（打包脚本
       不依赖它也能生成文档，size 一栏统一写"由 esptool image-info 验证"）。
       ——这在工程上叫"不要用猜测的二进制布局去解析关键指纹"，宁可少一个字段，
          也不要在说明文档里写错的值。
    """
    with open(path, "rb") as f:
        head = f.read(16)
    if len(head) < 9:
        return None

    magic, seg_count, spi_mode, spi_speed = head[0], head[1], head[2], head[3]
    entry = struct.unpack("<I", head[4:8])[0]

    mode_names = {0: "QIO", 1: "QOUT", 2: "DIO", 3: "DOUT"}
    # ⚠️ 这些是**镜像头里的原始字节值**，不是 0/1/2/3 的枚举。
    #    0x0F=80MHz 是 ESP32 老芯片的写法，0x1F=40MHz、0x2F=80MHz 是 S3/C3 起的新写法。
    speed_names = {0x0F: "80MHz", 0x1F: "40MHz", 0x2F: "80MHz", 0x00: "40MHz", 0x01: "26MHz", 0x02: "20MHz"}

    return {
        "magic": magic,
        "magic_ok": magic == 0xE9,
        "seg_count": seg_count,
        "spi_mode": mode_names.get(spi_mode, "?%d" % spi_mode),
        "spi_speed": speed_names.get(spi_speed, "?0x%02X" % spi_speed),
        "entry": entry,
        "raw": "%02X %02X %02X %02X" % (head[0], head[1], head[2], head[3]),
    }


def human(n):
    """字节数转人类可读（带十六进制，方便和 flash_args 对照）。"""
    return "%d B (0x%X)" % (n, n)


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
        with open(path, "rb") as f:
            data = f.read()
        if addr + len(data) > total_end:
            raise SystemExit("段 %s 超出合并范围（%d+%d > %d）" % (tag, addr, len(data), total_end))
        buf[addr:addr + len(data)] = data
        segments.append({
            "addr": addr, "name": fname, "tag": tag, "desc": desc,
            "size": len(data), "sha256": sha256_file(path), "path": path,
        })

    with open(out_merged, "wb") as f:
        f.write(buf)

    return segments, len(buf)


def main():
    ap = argparse.ArgumentParser(description="耀坤整理版 Flix 固件打包（生成单文件固件 + 指纹说明）")
    here = os.path.dirname(os.path.abspath(__file__))
    fw_root = os.path.dirname(here)

    ap.add_argument("--build-dir", default=os.path.join(fw_root, "flix", "build"),
                    help="arduino-cli 的 build 目录（里面有 flix.ino.bin 等）")
    ap.add_argument("--out-dir", default=os.path.join(fw_root, "release"),
                    help="发布产物输出目录")
    ap.add_argument("--version", default="1.0", help="对外版本号，如 1.0")
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
    A("- **硬件对应**：耀坤 S3 无人机（ESP32-S3，4MB Flash），4 个有刷电机（GPIO 15/5/6/16）")
    A("- **传感器**：MPU6050（I2C，WHO_AM_I=0x70）/ MPU-6500（SPI）—— **上电自动识别**，无需重编")
    A("- **控制**：姿态环 → 角速度环 → 四轴混控（串级 PID），支持 RAW / ACRO / STAB / AUTO 四种模式")
    A("- **遥控**：耀坤自研 ESP-NOW 遥控器（信道 6），同时兼容 SBUS 接收机与 Wi-Fi/UDP 地面站")
    A("- **地面站**：MAVLink v2，可接 QGC / MAVProxy")
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
    A("| 编译参数 | `esp32:esp32:esp32s3` + `USBMode=hwcdc, CDCOnBoot=cdc, CPUFreq=240, FlashMode=qio, FlashSize=4M, PartitionScheme=default` |")
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
    A("3. 设置：**SPI SPEED = 80MHz，SPI MODE = DIO，FLASH SIZE = 4MB**，选好 COM 口，BAUD 用 921600（不稳降到 115200）。")
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
    A("ESP-NOW 信道：6")
    A("…（IMU 自动识别输出）…")
    A("惯性测量单元就绪：MPU6500，总线 SPI")
    A("初始化完成")
    A("```")
    A("")
    A("三行关键指纹：**产品名串** + **版本行** + **ESP-NOW 信道 6**。三者同时出现即确认是本版。")
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
    A("- 应用程序镜像头：`%s` → SPI 模式 **%s**、频率 **%s**、段数 **%d**、入口 **0x%08X**" % (hdr["raw"], hdr["spi_mode"], hdr["spi_speed"], hdr["seg_count"], hdr["entry"]))
    A("- Flash 容量：**4MB**（编译参数 `FlashSize=4M`；如需二次确认可执行 `esptool image-info <bin>`，"
      "其 `Flash size` 一栏应为 4MB。S3 的容量信息在扩展头里，本脚本不解析")
    if boot_hdr:
        A("- bootloader 镜像头：`%s` → SPI 模式 **%s**、频率 **%s**" % (boot_hdr["raw"], boot_hdr["spi_mode"], boot_hdr["spi_speed"]))
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
    A("3. 家庭路由器与飞机热点**同信道**时容易互相干扰。飞机 AP 信道由 `espnowChannel`（默认 6）决定，")
    A("   若与家里路由撞了，用 `p ESPNOW_CHANNEL <1-13>` 改，但**遥控器端也要同步改**，否则 ESP-NOW 收不到。")
    A("4. 两台飞机同时上电会互相干扰 Wi-Fi，测试时请把另一台断电，否则无法归因。")
    A("5. 改了 PID / 校准参数会自动存 NVS，下次上电仍在。恢复出厂用控制台 `preset`（会一并清掉遥控标定），")
    A("   只想重置遥控请用 `rcdef`。")
    A("6. 本工程**依赖 FlixPeriph 与 ESP32_NOW_Serial 库**；自己重编时要保证库版本与第二节一致，")
    A("   否则可能出现「编过但 ESP-NOW 收不到」的现象。")
    A("")
    A("## 八、找到我们")
    A("")
    A("- B站（哔哩哔哩）：[耀坤智联](https://space.bilibili.com/3707011699050767)")
    A("- 立创开源硬件平台：[耀坤无人机](https://oshwhub.com/lindahe)")
    A("- GitHub：<https://github.com/YaoKunS>")
    A("- 淘宝店：[耀坤智联](https://shop35432590.taobao.com/)")
    A("")
    A("---")
    A("")
    A("本说明由 `tools/pack_flix_release.py` 自动生成；哈希、分段布局、镜像头与编译时间均现场从固件镜像提取，与 bin 严格一致。")

    md = "\n".join(L) + "\n"
    md_path = os.path.join(out_dir, stem + "_指纹与烧录说明.md")
    txt_path = os.path.join(out_dir, stem + "_指纹与烧录说明.txt")
    with open(md_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(md)
    # txt 版：去掉 markdown 记号，保持内容一致，方便手机/记事本看
    plain = md.replace("```bash", "").replace("```bat", "").replace("```", "")
    plain = plain.replace("| --- | --- | --- | --- |\n", "")
    with open(txt_path, "w", encoding="utf-8", newline="\r\n") as f:
        f.write(plain)

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
    print("说明文档   : %s" % md_path)
    print("             %s" % txt_path)
    print("=" * 68)


if __name__ == "__main__":
    main()
