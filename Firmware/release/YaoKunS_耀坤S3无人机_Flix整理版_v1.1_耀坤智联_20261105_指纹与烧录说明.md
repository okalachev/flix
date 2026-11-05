# 耀坤S3无人机 / Flix 整理版 —— 固件指纹与烧录说明

## 一、这份固件是什么

基于开源飞控 **Flix**（原作者 Oleg Kalachev）的**耀坤整理版**。在保留上游全部代码结构与 GPL 义务的前提下，针对耀坤 S3 硬件与自研 ESP-NOW 遥控做了适配。

- **上游项目**：<https://github.com/okalachev/flix>（`(C) Oleg Kalachev`，无独立 LICENSE 文件，版权归原作者）
- **整理版仓库**：<https://github.com/YaoKunS/flix>（分支 `yaokun-organized`，`master` 保持上游原版）
- **硬件对应**：耀坤 S3 无人机（ESP32-S3，8MB Flash），4 个有刷电机（GPIO 15/5/6/16）
- **传感器**：MPU6050（I2C，WHO_AM_I=0x70）/ MPU-6500（SPI）—— **上电自动识别**，无需重编
- **控制**：姿态环 → 角速度环 → 四轴混控（串级 PID），支持 RAW / ACRO / STAB / AUTO 四种模式
- **遥控**：耀坤自研 ESP-NOW 遥控器（信道 1），同时兼容 SBUS 接收机与 Wi-Fi/UDP 地面站
- **地面站**：MAVLink v2，可接 QGC / MAVProxy
- **v1.1 新特性**：① 出厂默认 **ESP-NOW + 热点共存**（旧版互斥，老板子 NVS 首次开机自动迁移）；
  ② 热点名后缀改读 **eFuse 出厂 STA MAC**（如 `YaoKun_F224`，与 `wifi` 命令里的 MAC 末两字节一致）；
  ③ 热点密码**三层校验**（NVS 脏值自动清洗 / 非法写入拒绝 / 出厂密码码表构造）
- **热点说明**：热点仅提供 **UDP 14550 的 MAVLink 遥测**，**没有 Web 网页**（浏览器打不开 192.168.4.1 是正常的）
- **灯语与音效**：红/绿/蓝三色灯（GPIO 41/7/14）+ 有源蜂鸣器（GPIO 39），非阻塞状态机
- **界面语言**：**默认全中文**（控制台、help 菜单、IMU 校准、初始化提示）；定义 `FLIX_LANG_EN` 可编英文版

> ★ 本固件为**纯姿态自稳 + 手动油门**：没有 GPS / 气压计 / 测距 / 光流，
> **松杆不会悬停**（姿态会稳平，但保持水平速度继续飘），**松油门就掉高度**。
> 首次试飞请在空旷场地、低空、装保护圈的前提下进行。

## 二、版本与来源

| 项目 | 内容 |
| --- | --- |
| 固件版本 | **v1.1**（耀坤整理版） |
| 品牌 | YaoKunS / 耀坤智联 / 耀坤无人机 |
| 编译时间 | Oct 18 2026 18:40:11 |
| 工具链 | arduino-cli 1.5.1 + esp32 core 3.3.8 |
| 编译参数 | `esp32:esp32:esp32s3:FlashSize=8M,PartitionScheme=default_8MB`（USB CDC 默认开启） |
| 第三方库 | FlixPeriph、ESP32_NOW_Serial（随 esp32 core）、MAVLink |
| 资源占用 | 应用程序 1058992 B (0x1028B0) |

> ⚠️ **读版本号的方式**：Arduino 构建的镜像里 `0x10020` 处的 `esp_app_desc_t`
> 由 ESP32 核心预编译库写入，`version` 字段是核心库标识，不代表本工程版本。
> 本固件版本以**开机横幅**打印的 `固件版本：v1.1（耀坤整理版）` 为准。

## 三、怎么烧（单文件，最简单）

### 方式 A：Flash Download Tool（图形界面）

1. 打开 Flash Download Tool，芯片选 **ESP32-S3**，工作模式 **Develop**。
2. 只添加这一个文件，地址填 **`0x0`**：`YaoKunS_耀坤S3无人机_Flix整理版_v1.1_耀坤智联_20261105_merged.bin`
3. 设置：**SPI SPEED = 80MHz，SPI MODE = DIO，FLASH SIZE = 8MB**，选好 COM 口，BAUD 用 921600（不稳降到 115200）。
4. ⚠️ **务必勾选 DoNotChgBin**。镜像头里已写好 SPI 模式/频率/容量与尾部 SHA-256，工具若改写头部会破坏校验导致无法开机。
5. 点 START，完成后按 RST 或重新上电。

### 方式 B：esptool 一条命令

```bash
esptool --chip esp32s3 --port COMx --baud 921600 write_flash 0x0 "YaoKunS_耀坤S3无人机_Flix整理版_v1.1_耀坤智联_20261105_merged.bin"
```

烧完若「不跑、停在下载模式」（日志出现 `waiting for download`）：GPIO0 被拉低没放开。
先 `--after no-reset` 连一下确认镜像可读，再执行 `esptool --after watchdog-reset run`，或拔插 USB / 按 RST。

## 四、怎么验证烧进去的是这一版（指纹探测）

**① 直接比文件哈希**（最可靠，不用上电）：

```bat
certutil -hashfile "YaoKunS_耀坤S3无人机_Flix整理版_v1.1_耀坤智联_20261105_merged.bin" SHA256
```

→ 应为 `7f8dafa31547670431049fc962e8d8b145b9fd7e88736607818a88ec4d4d2845`

**② 串口开机横幅**（115200，走**原生 USB-CDC 口**）。开机第一屏应为：

```
耀坤S3 飞控（Flix 整理版）启动中…
YaoKunS | 耀坤S3无人机 | Flix | 耀坤智联
固件版本：v1.1（耀坤整理版）
初始化参数存储
初始化电源检测
初始化无线通信
ESP-NOW 信道：1
…（IMU 自动识别输出）…
惯性测量单元就绪：MPU6500，总线 SPI
初始化完成
```

三行关键指纹：**产品名串** + **版本行** + **ESP-NOW 信道 1**。三者同时出现即确认是本版。

**③ 在 bin 里搜明文**（不用设备也能确认是 Flix 整理版）：

- 搜 `耀坤S3 飞控（Flix 整理版）启动中` → 命中（中文开机横幅，英文版无此串）
- 搜 `YaoKunS | 耀坤S3无人机 | Flix | 耀坤智联` → 命中 1 次（指纹串）
- 搜 `esp_now_register_recv_cb` 的调用痕迹不易搜，改搜 `接收丢弃字节数` → 命中（原生 ESP-NOW 接收统计）

**④ 读 flash 里的镜像头**（只读，不改动任何东西）：

```bash
esptool --chip esp32s3 --port COMx read_flash 0x10000 0x200 app_hdr.bin
```

→ 首字节应为 `E9`（ESP 镜像魔数）。

## 五、产物与校验（SHA-256）

| 产物 | 烧写地址 | SHA-256 | 大小 |
| --- | --- | --- | --- |
| **merged 单文件固件** | `0x0` 整体刷 | `7f8dafa31547670431049fc962e8d8b145b9fd7e88736607818a88ec4d4d2845` | 1124528 B (0x1128B0) |
| 二级引导程序 | `0x0` | `6f232c4aafe7d21239a1c59404723ca2cfdca063ef73501a21126a8cef39f432` | 19984 B (0x4E10) |
| 分区表 | `0x8000` | `6a88d59601a83a16a19a08114b59d338324b8dec267d8b43e5d61ad56ec92102` | 3072 B (0xC00) |
| boot_app0 | `0xE000` | 由 esp32 core 3.3.8 提供（把 otadata 选到 app0，缺它不启动） | 8192 B (0x2000) |
| 应用程序 | `0x10000` | `ad61033964935de7092b2b75b09b12829a6ebee115f0fe5d766faf72f216f54a` | 1058992 B (0x1028B0) |

- 合并布局：`0x0 bootloader + 0x8000 分区表 + 0xE000 boot_app0 + 0x10000 应用程序`（段间空隙填 0xFF）
- 应用程序镜像头：`E9 06 02 3F` → SPI 模式 **DIO**、频率 **80MHz**、Flash 容量 **8MB**、段数 **6**、入口 **0x40379F5C**
- ⚠️ 本包按 **8MB Flash** 编译（分区表 default_8MB），**4MB 板不可烧写本包**，请用源码改 `FlashSize=4M,PartitionScheme=default` 重编
- Flash 容量：**8MB**（编译参数 `FlashSize=8M`；如需二次确认可执行 `esptool image-info <bin>`，其 `Flash size` 一栏应为 8MB。容量指纹取自镜像头 offset 3 高半字节，见上方镜像头行）
- bootloader 镜像头：`E9 03 02 3F` → SPI 模式 **DIO**、频率 **80MHz**、Flash 容量 **8MB**

## 六、分片烧写（保住 NVS 里的标定数据）

**单文件从 0x0 整片刷写会把 `0x9000~0xE000` 的 nvs 区写成 0xFF = 清空**，
也就是：WiFi 热点名/密码、IMU 自动识别结果、遥控端点标定值全部丢失。

新板/空板无所谓；**自己已经标定好的机器**请改用分片写，跳过 nvs：

```bash
esptool --chip esp32s3 --port COMx --baud 921600 write_flash \
  0x0 "flix.ino.bootloader.bin" \
  0x8000 "flix.ino.partitions.bin" \
  0x10000 "flix.ino.bin" \
```

（`boot_app0.bin` 位于 esp32 core 的 `tools/partitions/` 下，一般不重刷也没事。）

## 七、六点须知

1. **首次烧完请重跑 `cal`（IMU 校准）与遥控标定**：整片刷写清空了 NVS，IMU 零偏需要重新测。
2. 控制台走**原生 USB 口**（`CDCOnBoot=cdc`）。只插板上 UART 口看不到日志是正常的。
3. 家庭路由器与飞机热点**同信道**时容易互相干扰。飞机 AP 信道由 `espnowChannel`（默认 1）决定，
   若与家里路由撞了，用 `p ESPNOW_CHANNEL <1-13>` 改，但**遥控器端也要同步改**，否则 ESP-NOW 收不到。
4. 两台飞机同时上电会互相干扰 Wi-Fi，测试时请把另一台断电，否则无法归因。
5. 改了 PID / 校准参数会自动存 NVS，下次上电仍在。恢复出厂用控制台 `preset`（会一并清掉遥控标定），
   只想重置遥控请用 `rcdef`。
6. 本工程**依赖 FlixPeriph 与 ESP32_NOW_Serial 库**；自己重编时要保证库版本与第二节一致，
   否则可能出现「编过但 ESP-NOW 收不到」的现象。

## 八、连飞机热点（AP）

热点名 = `YaoKun_` + 芯片出厂 STA MAC 末两字节（如 `YaoKun_F224`），开机日志里 **`当前热点名：`** 那行为准。

出厂密码：**`1-2-3-4-5-6-7-8`**（中文逐位读作：一、二、三、四、五、六、七、八）。

> 🔑 **连字符只是分隔符，输入时不要带** —— 实际是 8 个字符，逐位敲：

| 位次 | 第 1 位 | 第 2 位 | 第 3 位 | 第 4 位 | 第 5 位 | 第 6 位 | 第 7 位 | 第 8 位 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 该位字符 | `1` | `2` | `3` | `4` | `5` | `6` | `7` | `8` |

> ⚠️ 全文档统一用**带连字符**的形式书写，刻意不写连续的 8 位明文：连续数字串经传输/渲染通道
> 会被改写（本工程已多次实测，v1.0 文档就因此写错过密码，导致所有复刻用户连不上热点）。
> 输入时按上表逐位敲，共 8 位，中间不加空格或符号。
>
> 源码里的默认值用 ASCII 码表 `{49,50,51,52,53,54,55,56,0}` 构造，仓库内没有任何明文密码串。

热点只提供 **UDP 14550 的 MAVLink**（QGC / MAVProxy 用），**没有网页**：
浏览器打开 `192.168.4.1` 空白/拒绝连接都是**正常现象**，不代表没连上。
判断是否连上请看 `wifi` 命令的 `已连设备数`（≥1 即 AP 侧已登记）。

## 九、找到我们

- B站（哔哩哔哩）：[耀坤智联](https://space.bilibili.com/3707011699050767)
- 立创开源硬件平台：[耀坤无人机](https://oshwhub.com/lindahe)
- GitHub：<https://github.com/YaoKunS>
- 淘宝店：[耀坤智联](https://shop35432590.taobao.com/)

---

本说明由 `tools/pack_flix_release.py` 自动生成；哈希、分段布局、镜像头与编译时间均现场从固件镜像提取，与 bin 严格一致。
