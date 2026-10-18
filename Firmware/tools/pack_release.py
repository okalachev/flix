#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
耀坤 S3 / Flix 整理版 —— 单文件固件打包脚本
=============================================
把 arduino-cli 编出来的分段 bin 合并成一个"从 0x0 一路写完"的单文件固件，
并生成配套的《固件指纹与烧录说明》（.md 与 .txt 各一份）。

【为什么要单文件】
arduino-cli 正常产出 4 个分段文件，烧录时要按偏移逐个写：
    0x0      bootloader.bin
    0x8000   partitions.bin
    0xE000   boot_app0.bin
    0x10000  flix.ino.bin
对不熟悉 esptool 的人容易写错偏移、漏写某段。合并成一个文件后，只需：

    esptool --chip esp32s3 --port COMxx write_flash 0x0 <单文件.bin>

【🔴 合并时的 flash-mode 必须用 dio】
bootloader 头部固定写的是 DIO 模式。若用 qio 合并，esptool 会改写 bootloader
头（0x2 字节 00→02）并重算尾部 SHA256，结果与原厂镜像差 33 字节，
个别板子会因此起不来。用 dio 合并后，紧凑镜像与 arduino-cli 官方镜像前缀
逐字节一致（已验证）。

【用法】
    python pack_release.py                 # 用默认版本 v1.0 打包
    python pack_release.py --ver v1.1      # 指定版本号
    python pack_release.py --no-merge      # 只重生成说明，不重新合并

【依赖】esptool（随 Arduino ESP32 core 安装，用 python -m esptool 调用）
"""

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
from datetime import datetime


# ── 路径常量 ────────────────────────────────────────────────────────────────
# 全部用相对本脚本的路径推导，换机器/换目录都不用改。
TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))          # .../Firmware/tools
FIRMWARE_DIR = os.path.dirname(TOOLS_DIR)                        # .../Firmware
FLIX_DIR = os.path.join(FIRMWARE_DIR, "flix")                    # .../Firmware/flix
BUILD_DIR = os.path.join(FLIX_DIR, "build", "esp32.esp32.esp32s3")
RELEASE_DIR = os.path.join(FIRMWARE_DIR, "release")

# 品牌与命名
BRAND_CN = "耀坤智联"
BRAND_EN = "YaoKunS"
PRODUCT_CN = "耀坤S3无人机_Flix整理版"
REPO_URL = "https://github.com/YaoKunS/flix"
UPSTREAM_URL = "https://github.com/okalachev/flix"
TAOBAO_URL = "https://shop35432590.taobao.com/"
BILIBILI_URL = "https://space.bilibili.com/3707011699050767/"


def sha256_of(path):
    """算文件 SHA-256。分块读，避免大文件占内存。"""
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def human_size(n):
    """字节数 → 带千分位与十六进制的可读串，方便肉眼比对。"""
    return f"{n:,} B (0x{n:X})"


def read_build_info():
    """从 build.options.json 与 build 目录里读编译信息。
    【为什么从这里读而不写死】FQBN、库路径随机器不同，写死会误导用户。"""
    info = {
        "fqbn": "unknown",
        "libs": "unknown",
        "build_time": "unknown",
        "app_size": 0,
        "static_size": 0,
    }
    opt = os.path.join(BUILD_DIR, "build.options.json")
    if os.path.isfile(opt):
        with open(opt, "r", encoding="utf-8") as f:
            d = json.load(f)
        info["fqbn"] = d.get("fqbn", "unknown")
        info["libs"] = d.get("otherLibrariesFolders", "unknown")

    # 编译时间取 app bin 的 mtime（就是这次编译产出的时刻）
    app = os.path.join(BUILD_DIR, "flix.ino.bin")
    if os.path.isfile(app):
        info["build_time"] = datetime.fromtimestamp(os.path.getmtime(app)).strftime("%Y-%m-%d %H:%M:%S")
        info["app_size"] = os.path.getsize(app)

    # 静态内存占用从 map 文件末尾抓不到，退而从 elf 大小估计意义不大，
    # 这里改为读 arduino-cli 留在同目录的 .elf 文件名无关信息 → 留空由说明模板处理
    return info


def find_boot_app0():
    """定位 boot_app0.bin。
    【为什么不在 build 目录里找】它是 esp32 core 自带的公共镜像（OTA 数据区初始内容），
    位于 packages/esp32/hardware/esp32/<ver>/tools/partitions/ 下，
    arduino-cli 导出时不一定复制到 build 目录。这里按 core 版本逐个扫。"""
    import glob
    pats = [
        os.path.join(os.environ.get("LOCALAPPDATA", ""),
                     "Arduino15", "packages", "esp32", "hardware", "esp32", "*",
                     "tools", "partitions", "boot_app0.bin"),
    ]
    for p in pats:
        hits = sorted(glob.glob(p))
        if hits:
            return hits[-1]  # 取版本号最大的
    # 退路：在 build 目录里找
    local = os.path.join(BUILD_DIR, "boot_app0.bin")
    return local if os.path.isfile(local) else None


def do_merge(out_path):
    """调用 esptool merge_bin 合并分段。
    【关键】--flash-mode dio，理由见文件头注释。"""
    boot_app0 = find_boot_app0()
    if boot_app0 is None:
        print("[错误] 找不到 boot_app0.bin（esp32 core 的 tools/partitions/ 下）")
        return False

    parts = [
        (0x0, os.path.join(BUILD_DIR, "flix.ino.bootloader.bin")),
        (0x8000, os.path.join(BUILD_DIR, "flix.ino.partitions.bin")),
        (0xE000, boot_app0),
        (0x10000, os.path.join(BUILD_DIR, "flix.ino.bin")),
    ]
    for _, p in parts:
        if not os.path.isfile(p):
            print(f"[错误] 缺少分段文件：{p}")
            print("       请先在 Firmware/ 目录执行 arduino-cli compile（带 --export-binaries）")
            return False

    cmd = [
        sys.executable, "-m", "esptool", "--chip", "esp32s3",
        "merge-bin", "-o", out_path,
        "--flash-mode", "dio",
        "--flash-freq", "80m",
        "--flash-size", "4MB",
    ]
    # 【esptool 5.x 语法】地址与文件名必须是**两个独立参数**，
    # 不能写成 "0x0 file.bin" 一个字符串（4.x 可以，5.x 会报 "must be a number"）。
    for off, p in parts:
        cmd.append(f"0x{off:x}")
        cmd.append(p)

    print("[合并] " + " ".join(cmd[:6]) + " ...")
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        print("[错误] merge_bin 失败：")
        print(r.stdout[-2000:])
        print(r.stderr[-2000:])
        return False
    print(f"[合并] 完成 → {out_path}")
    return True


def build_notes_md(ver, meta):
    """生成 Markdown 版说明。内容涵盖：产品定位、版本来源、指纹、烧录、
    首次上电、遥控配对、飞行边界、故障速查。"""
    return f"""# 耀坤S3无人机 / Flix 整理版 —— 固件指纹与烧录说明

## 一、这份固件是什么

基于开源飞控 **Flix**（原作者 Oleg Kalachev）的**耀坤整理版**。在保留上游全部代码结构与版权声明的前提下，针对耀坤 S3 硬件与自研 ESP-NOW 遥控做了适配，并把界面**全面中文化**。

- **上游项目**：<{UPSTREAM_URL}>（`(C) Oleg Kalachev`）
- **整理版仓库**：<{REPO_URL}>（分支 `yaokun-organized`，`master` 保持上游原版）
- **硬件对应**：耀坤 S3 无人机（ESP32-S3）
- **电机**：4 个有刷电机，GPIO **15 / 5 / 6 / 16**（RL / RR / FR / FL），PWM **20 kHz**
- **传感器**：MPU-6050（I2C）与 MPU-6500（SPI）—— **上电自动识别，无需重编**
- **遥控**：耀坤自研 ESP-NOW 遥控器（**信道 1**）；同时兼容 SBUS 接收机与 Wi-Fi/UDP 地面站
- **地面站**：MAVLink v2（可接 QGC / MAVProxy）
- **灯语**：红/绿/蓝三色灯（GPIO 41/7/14）+ 有源蜂鸣器（GPIO 39）
- **界面语言**：**默认全中文**；定义 `FLIX_LANG_EN` 可编英文版

> ★ **飞行能力边界（务必先读）**
>
> 本固件是**纯姿态自稳 + 手动油门**：没有 GPS、气压计、测距、光流等任何位置传感器。
>
> - **松杆不会悬停** —— 姿态会稳平，但保持水平速度继续飘
> - **松油门就掉高度**
> - 首次试飞请在**空旷场地、低空、装保护圈**的前提下进行

## 二、版本与来源

| 项目 | 内容 |
| --- | --- |
| 固件版本 | **{ver}**（耀坤整理版） |
| 品牌 | {BRAND_CN} / {BRAND_EN} |
| 编译时间 | {meta['build_time']} |
| 工具链 | arduino-cli + esp32 core 3.3.8 |
| 编译参数 | `{meta['fqbn']}` |
| 第三方库 | FlixPeriph、ESP32_NOW_Serial（随 core）、MAVLink |
| 界面语言 | 中文（默认） |
| 源文件规模 | i18n 文案 163 条 × 2 语言 |

## 三、固件指纹（用于确认拿到的是哪一版）

```
文件名  ：{meta['filename']}
大小    ：{meta['bin_size_h']}
SHA-256 ：{meta['bin_sha']}
```

**校验方法（Windows）**：

```cmd
certutil -hashfile "{meta['filename']}" SHA256
```

**校验方法（Linux / macOS）**：

```bash
sha256sum "{meta['filename']}"
```

> 输出应与上面 SHA-256 完全一致。**不一致说明文件损坏或被改动，请勿烧录。**

## 四、烧录方法

### 4.1 前置条件

- ESP32-S3 开发板（耀坤 S3 无人机）
- USB 数据线（**必须能传数据**，很多线只能充电）
- Python 环境 + `esptool`（`pip install esptool`）
- 或使用图形工具：**Flash Download Tool**（乐鑫官方）

### 4.2 确认串口号

插上 USB 后，在「设备管理器 → 端口」查看新增的 COM 口（如 `COM28`）。
耀坤两块板的识别特征：

| 板子 | 芯片 MAC | 说明 |
| --- | --- | --- |
| 立创 ESP32-S3 R8N8 | `80:65:99:DF:F2:24` | 会额外出现一个 CH340K 口，**控制台不走它** |
| 淘宝 ESP32-S3 N16R8 | `E0:72:A1:F2:BF:CC` | 只有一个原生 USB 口 |

> ⚠️ **COM 号会随插拔顺序变化**，认板请看 MAC，不要认 COM 号。

### 4.3 一条命令烧录（推荐）

```bash
esptool --chip esp32s3 --port COM28 --baud 460800 \\
        --before default_reset --after watchdog-reset \\
        write_flash 0x0 "{meta['filename']}"
```

参数说明：

| 参数 | 作用 |
| --- | --- |
| `--baud 460800` | 烧录速率。若报错可降到 `115200` |
| `--before default_reset` | 自动进下载模式 |
| `--after watchdog-reset` | **烧完用看门狗复位**。比 `hard_reset` 可靠 —— 有些板子 GPIO0 被拉低，`hard_reset` 会反复落回下载模式 |

### 4.4 烧录成功的判断

看到这两行即成功：

```
Wrote 4194304 bytes ... at 0x00000000
Hash of data verified.
```

最后一行是 `Hash of data verified.` 才说明**写入校验通过**。

### 4.5 如果烧完停在下载模式

现象：串口没有任何输出，像块砖。

排查顺序：

1. 用 `esptool --chip esp32s3 --port COM28 chip-id` 看能否通信
2. 回读首字节确认固件在：`esptool ... read_flash 0x10000 1 out.bin`，应为 `0xE9`
3. 若仍在下载模式，改用 `--after watchdog-reset`（本说明已默认使用）
4. 换一根**能传数据**的 USB 线

## 五、首次上电

### 5.1 开机横幅

正常开机会在控制台打印（中文版）：

```
==============================================================
  耀坤S3 飞控（Flix 整理版）   YaoKunS Flight Controller
  硬件：立创 ESP32-S3 + MPU6500/MPU6050   遥控：ESP-NOW
--------------------------------------------------------------
  淘宝：耀坤智联  {TAOBAO_URL}
  B 站：耀坤智联  {BILIBILI_URL}
--------------------------------------------------------------
  内核 (C) Oleg Kalachev  |  https://github.com/okalachev/flix
```

> **控制台走原生 USB 口**（`303A:1001`），不是 CH340K 口。
> 部分主机需要把串口的 **DTR 拉高**才有输出。

### 5.2 确认 IMU 识别

```
imu
```

期望输出包含：

```
状态：正常
型号：MPU-6500        ← 或 MPU6050
芯片标识（WHO_AM_I）：0x70    ← 6050 系是 0x68 或 0x70
总线：SPI             ← 或 I2C0
```

两种模块的引脚（程序自动识别，无需设置）：

| 模块 | 总线 | 引脚 |
| --- | --- | --- |
| MPU-6500 | SPI | SCK=38 MISO=21 MOSI=18 CS=17（候选 17/42 自动兼容新旧 PCB） |
| MPU-6050 | I2C0 | SDA=9 SCL=8 |

### 5.3 确认主循环频率

```
time
```

期望 `主循环频率：1000 Hz`、`时间回绕次数：0`。

## 六、遥控器配对（**每次重启都要做**）

耀坤中文版 ESP-NOW 遥控器，操作顺序**不可省略**：

1. 飞机上电，等横幅打印完
2. 遥控器进入「**对端发现**」页
3. **点选**目标飞机
4. **按遥控器的「左按钮（辅助按钮）」使能**

> 🔴 **第 4 步最关键，且界面上没有这个开关** —— 它是一个纯物理按键。
> 不按左按钮，遥控器照样发 `MANUAL_CONTROL`，但四轴数值全是中位（≈0.01），
> 表现为「有包、有帧、msgid=69，`rc` 就是不动」。

### 6.1 用 `wifi` 命令诊断链路

```
wifi
```

按这个顺序判读：

| 现象 | 结论 |
| --- | --- |
| `接收包数` = 0 | 物理层不通 → **查信道**（飞机/遥控器都应是 1） |
| 包数涨，`已解析 MAVLink 帧数` = 0 | 协议不匹配 |
| 帧数涨，`最近一帧消息 ID` ≠ 69 | 遥控没发 `MANUAL_CONTROL` |
| msgid = 69，但 `rc` 四轴全 0.01 | **遥控器未使能** → 按左按钮 |
| 全对，四轴跟摇杆动 | ✅ 链路正常 |

正常时的参考数据：**21782 包 / 21782 帧（100% 解析）**、msgid=69、sysid=127。

### 6.2 换信道

```bash
ch 1      # 设信道 1（默认）
ch 6      # 设信道 6
reboot    # 重启生效
```

> ⚠️ 飞机和遥控器**必须同信道**。中文版遥控不调 `esp_wifi_set_channel`，
> 走 Wi-Fi 默认信道 **1**，所以飞机端也取 1。
> **注意**：遥控器「对端发现」页能看到飞机 ≠ 数据能通（扫描是跨信道的）。

## 七、控制台常用命令

| 命令 | 作用 |
| --- | --- |
| `help` | 列出全部命令 |
| `imu` | IMU 状态、零偏、比例系数、原始值 |
| `st` | 姿态（横滚/俯仰/偏航）、四元数 |
| `rc` | 遥控通道原始值与归一化值 |
| `mot` | 四路电机输出（0~1） |
| `wifi` | ESP-NOW 链路诊断（包数/帧数/msgid/MAC） |
| `time` | 时间基准、主循环频率、回绕次数 |
| `arm` / `disarm` | 解锁 / 上锁 |
| `p <参数名> <值>` | 设置参数 |
| `p <参数名>` | 读取参数 |
| `preset` | 恢复**全部**默认参数（慎用） |
| `ca` | 六面加速度校准（见下方警告） |
| `mfr` / `mfl` / `mrr` / `mrl` | 单独测试某个电机 |
| `reboot` | 重启 |

### 7.1 电机位置对应（实测确认）

| 命令 | 对应电机 | 引脚 |
| --- | --- | --- |
| `mrr` | 右后 | 5 |
| `mrl` | 左后 | 15 |
| `mfr` | 右前 | 6 |
| `mfl` | 左前 | 16 |

### 7.2 🔴 关于 `ca` 六面校准

> **警告：当前版本的 `ca` 是「48 秒一口气跑完」，中间没有任何等待输入的停顿。**
>
> 执行后固件会连续打印「第 1/6 步」到「第 6/6 步」，每步 8 秒。
> **人手跟不上** —— 若没能在每个 8 秒窗口内把机身翻到位，
> 六次采样会落在同一姿态，`加速度比例系数` 会塌到 `0.02` 量级（正常应 ≈ `1.0`），
> 姿态数据被放大数十倍而彻底失真。
>
> **建议：不要轻易执行 `ca`。** MPU-6500 / MPU-6050 的出厂精度足够直接飞行。

若已误执行并导致数据异常，用六条命令恢复（**不用重刷固件**）：

```bash
p IMU_ACC_SCALE_X 1
p IMU_ACC_SCALE_Y 1
p IMU_ACC_SCALE_Z 1
p IMU_ACC_BIAS_X 0
p IMU_ACC_BIAS_Y 0
p IMU_ACC_BIAS_Z 0
```

执行后姿态立刻归零（实测横滚 0.015° / 俯仰 −0.004°）。

> 💡 另一个容易误判的点：`imu` 打印的「采样频率」是 **主循环频率**，
> **不是 IMU 输出速率**。校准期间显示 20 Hz、空闲显示 1 Hz 都是正常的，
> 不要因此去改 `configureIMU()`。

## 八、常见问题速查

| 现象 | 最可能原因 | 处理 |
| --- | --- | --- |
| 串口完全没输出 | 用了 CH340K 口 / DTR 未拉高 / 固件没烧进去 | 换原生 USB 口、拉高 DTR、重新烧录 |
| 能开机，但电机不转 | 未解锁 / 油门在低位 | 油门收到底再解锁 |
| 遥控连上但摇杆不动 | **遥控器未按左按钮使能** | 按左按钮（见第六节） |
| `wifi` 包数恒为 0 | 信道不一致 | 两端都设 `ch 1` |
| 电机某一侧明显偏快 | 机身没放平 / 姿态零偏 | 桌面静置看 `st`，横滚俯仰应≈0 |
| IMU 识别不到 | 排母松动 / 模块损坏 | 看 `imu` 输出，重插排母 |
| 飞起来往一边飘 | **正常现象** —— 无位置传感器，不会自主悬停 | 手动修正 |

## 九、版权与致谢

- 飞控内核：**Flix** by Oleg Kalachev — <{UPSTREAM_URL}>
- 本整理版：**{BRAND_CN}（{BRAND_EN}）** — <{REPO_URL}>
- 淘宝店铺：<{TAOBAO_URL}>
- B 站主页：<{BILIBILI_URL}>

上游 Flix 项目未附独立 LICENSE 文件，版权归原作者所有。本整理版保留全部原始版权声明，
改动部分遵循上游项目的开源精神公开发布。

---

*本说明由 `Firmware/tools/pack_release.py` 自动生成，请勿手工编辑。*
"""


def main():
    ap = argparse.ArgumentParser(description="耀坤S3 / Flix 整理版固件打包")
    ap.add_argument("--ver", default="v1.0", help="版本号，默认 v1.0")
    ap.add_argument("--no-merge", action="store_true", help="跳过合并，只重生成说明")
    args = ap.parse_args()

    os.makedirs(RELEASE_DIR, exist_ok=True)

    date_str = datetime.now().strftime("%Y%m%d")
    base = f"{BRAND_EN}_{PRODUCT_CN}_{args.ver}_{BRAND_CN}_{date_str}"
    bin_path = os.path.join(RELEASE_DIR, base + "_merged.bin")

    if not args.no_merge:
        if not do_merge(bin_path):
            return 1
    elif not os.path.isfile(bin_path):
        print(f"[错误] --no-merge 模式下找不到已存在的 {bin_path}")
        return 1

    meta = {
        "filename": os.path.basename(bin_path),
        "bin_size": os.path.getsize(bin_path),
        "bin_size_h": human_size(os.path.getsize(bin_path)),
        "bin_sha": sha256_of(bin_path),
        "build_time": read_build_info()["build_time"],
        "fqbn": read_build_info()["fqbn"],
    }

    md_text = build_notes_md(args.ver, meta)

    md_path = os.path.join(RELEASE_DIR, base + "_指纹与烧录说明.md")
    with open(md_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(md_text)

    # txt 版：去掉 Markdown 标记，做成纯文本便于双击查看
    txt = md_text
    txt = re.sub(r"^#{1,6}\s*", "", txt, flags=re.MULTILINE)   # 去标题井号
    txt = txt.replace("**", "").replace("`", "")
    txt = re.sub(r"^\| ", "", txt, flags=re.MULTILINE)
    txt = re.sub(r"^\|", "", txt, flags=re.MULTILINE)
    txt = txt.replace(" | ", "  |  ").replace("---", "-")
    txt = re.sub(r"^> ?", "", txt, flags=re.MULTILINE)         # 去引用符
    txt = txt.replace("\n\n\n", "\n\n")

    txt_path = os.path.join(RELEASE_DIR, base + "_指纹与烧录说明.txt")
    with open(txt_path, "w", encoding="utf-8", newline="\r\n") as f:
        f.write(txt)

    print()
    print("=" * 72)
    print("  打包完成")
    print("=" * 72)
    print(f"  固件    : {os.path.basename(bin_path)}")
    print(f"  大小    : {meta['bin_size_h']}")
    print(f"  SHA-256 : {meta['bin_sha']}")
    print(f"  说明    : {os.path.basename(md_path)}")
    print(f"            {os.path.basename(txt_path)}")
    print("=" * 72)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
