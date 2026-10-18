<div align="center">

# 🚁 Flix 飞控 · 耀坤整理版

**从零实现的 ESP32 开源四轴飞控 —— 固件不足 2000 行 Arduino 代码，专为教学与研究而生**

![MCU](https://img.shields.io/badge/MCU-ESP32%20%7C%20ESP32--S3%20%7C%20ESP32--C3-purple)

![Framework](https://img.shields.io/badge/Framework-Arduino-blue)

![Protocol](https://img.shields.io/badge/Protocol-MAVLink%20%7C%20ESP--NOW-green)

![Language](https://img.shields.io/badge/Language-C%2B%2B17-orange)

![Upstream](https://img.shields.io/badge/Upstream-okalachev%2Fflix-lightgrey)

![B站](https://img.shields.io/badge/B%E7%AB%99-%E8%80%80%E5%9D%A4%E6%99%BA%E8%81%94-ff69b4)

**简体中文** · [**English**](README_EN.md)

</div>

---

> 📌 **本仓库是 [okalachev/flix](https://github.com/okalachev/flix) 的中文整理版 + 耀坤 S3 适配版。**  
> `master` 分支**保持上游原样**（仅调整目录位置，未改一行代码逻辑）。  
> `yaokun-organized` 分支在整理版基础上**新增了硬件适配与中文界面**（详见 [耀坤 S3 适配版](#-耀坤-s3-适配版yakun-organized-分支)）。  
> 上游项目与作者：[Oleg Kalachev](https://github.com/okalachev) · [项目官网 quadcopter.dev](https://quadcopter.dev)

## 📌 这是什么

Flix 是一款**专为教育和科研设计**的开源四轴飞行器固件，运行在 ESP32 系列芯片上。

它的最大特色是**极简**：固件全部用 Arduino C/C++ 写成，**不足 2000 行**，  
没有 RTOS、没有闭源库、没有黑盒算法 —— 传感器驱动、姿态解算、PID 控制、电机混控、  
MAVLink 通信全部摊开在你面前，读完就能真正理解飞控是怎么工作的。

![Flix Gazebo 仿真器](Docs/img/simulator1.png)

## ✨ 核心特性

| 特性             | 说明                                                      |
| -------------- | ------------------------------------------------------- |
| **极简源码**       | Arduino 编写，固件 **< 2000 行**，全部逻辑可读可改                     |
| **通用元件**       | 全部采用市面常见元件，无需定制打样即可组装                                   |
| **MAVLink 通信** | 通过 Wi-Fi 或 ESP-NOW 使用 MAVLink 协议，可用 QGroundControl 等地面站 |
| **多种操控方式**     | USB 手柄 / RC 遥控器 / 手机，Wi-Fi 与 ESP-NOW 双链路                |
| **无线命令行**      | 无需连接电脑，直接用手机/电脑连上 Wi-Fi 就能下发命令、查看状态                     |
| **Gazebo 仿真**  | 仿真器跑的是**同一份 Arduino 源码**，改固件不用等烧板子                      |
| **Python 库**   | 提供 `pyflix` 库，可用脚本编排自动飞行                                |
| **配套教材**       | 飞控理论与实践的系统教材（[持续编写中](https://quadcopter.dev)）           |
| **多芯片支持**      | ESP32 / ESP32-S3 / ESP32-C3 一份代码通吃，换板子只改 `BOARD`        |

## 🚁 真的能飞

实机演示视频：<https://youtu.be/hT46CZ1CgC4>

<a href="https://youtu.be/hT46CZ1CgC4"><img width=500 src="https://i3.ytimg.com/vi/hT46CZ1CgC4/maxresdefault.jpg"></a>

Version 0 演示视频：<https://youtu.be/8GzzIQ3C6DQ>

<a href="https://youtu.be/8GzzIQ3C6DQ"><img width=500 src="https://i3.ytimg.com/vi/8GzzIQ3C6DQ/maxresdefault.jpg"></a>

教育应用（RoboCamp 2025）：<https://youtu.be/Wd3yaorjTx0>

<a href="https://youtu.be/Wd3yaorjTx0"><img width=500 src="https://i3.ytimg.com/vi/Wd3yaorjTx0/sddefault.jpg"></a>

社区作品展：[Docs/user.md](Docs/user.md)

<a href="Docs/user.md"><img src="Docs/img/user/user.jpg" width=500></a>

## 📁 目录结构（本整理版重新整理）

> 🔧 **为什么要重整**：上游仓库把固件、文档、工具脚本平铺在根目录，  
> 目录里同时存在 `.ino` 源码、Python 脚本、mdBook 教材和 Gazebo 模型，容易看花。  
> 本整理版按"**用途**"重新分层，**固件相关的一律收进 `Firmware/`**。  
> 重要的是：`Firmware/` 内部是**一个完整可独立构建的工程**，  
> 所以在根目录执行 `make` 依然能直接编译，不需要改任何构建参数。

```text
flix/                          # ← 仓库根：说明文档 + 转发用 Makefile
├── README.md                  # 本文件（中文）
├── README_EN.md               # 英文版说明
├── Makefile                   # 转发器：把根目录的 make 转发给 Firmware/Makefile
├── Firmware/                  # ★ 固件工程（可独立构建）
│   ├── Makefile               #   真正的构建脚本（编译 / 烧录 / 仿真）
│   ├── flix/                  #   Arduino sketch —— 飞控全部源码
│   │   ├── flix.ino           #     主入口：setup / loop
│   │   ├── config.h           #     引脚定义与飞行参数
│   │   ├── i18n.h             #     ★ 界面文案表（中英双语，135 条，唯一数据源）
│   │   ├── imu.ino            #     IMU 驱动、自动识别与校准
│   │   ├── estimate.ino       #     姿态解算
│   │   ├── control.ino        #     PID 控制与电机混控
│   │   ├── motors.ino         #     电机 / MOSFET 驱动
│   │   ├── rc.ino             #     遥控器输入
│   │   ├── mavlink.ino        #     MAVLink 协议
│   │   ├── wifi.ino           #     Wi-Fi 与 ESP-NOW 通信
│   │   ├── console.ino        #     无线命令行
│   │   ├── led.ino / power.ino / log.ino / safety.ino / time.ino / parameters.ino
│   │   └── pid.h / filter.h / quaternion.h / vector.h / util.h
│   ├── tools/                 #   ★ 发布打包脚本
│   │   └── pack_flix_release.py   # 合并单文件固件 + 自动生成 SHA-256 指纹说明
│   ├── release/               #   ★ 发布产物（merged.bin + 指纹与烧录说明）
│   └── gazebo/                #   Gazebo 仿真器（跑同一份 .ino 源码）
│       ├── flix.world         #     仿真世界
│       ├── simulator.cpp      #     仿真主程序
│       └── models/            #     机架 / 场景模型
├── Docs/                      # ★ 文档与教材
│   ├── 耀坤S3适配说明.md       #   ★ 改动清单 / 根因分析 / 验证记录
│   ├── flix技术说明书.html     #   ★ 图文版技术说明书
│   ├── assembly.md            #   组装说明
│   ├── usage.md               #   编译 / 调试 / 飞行
│   ├── firmware.md            #   固件架构总览（强烈推荐先读）
│   ├── troubleshooting.md     #   故障排查
│   ├── log.md                 #   日志分析
│   ├── user.md                #   社区作品展
│   └── book/                  #   mdBook 飞控教材
├── Tools/                     # ★ 工具脚本
│   ├── pyflix/                #   Python 飞控库
│   ├── espnow-proxy/          #   ESP-NOW 代理固件
│   ├── csv_to_ulog/           #   CSV → ULog 转换
│   └── log.py / fft.py / test.py ...
└── .github/workflows/         # CI：编译验证 + 文档构建 + 工具测试
```

**整理前后对照：**

| 原始路径       | 整理后路径                         | 理由                  |
| ---------- | ----------------------------- | ------------------- |
| `flix/`    | `Firmware/flix/`              | Arduino 固件源码        |
| `gazebo/`  | `Firmware/gazebo/`            | 仿真器跑同一份固件，属固件工程一部分  |
| `Makefile` | `Firmware/Makefile`（+ 根目录转发器） | 构建脚本跟固件走；根目录留转发器保兼容 |
| `docs/`    | `Docs/`                       | 文档                  |
| `tools/`   | `Tools/`                      | 工具脚本                |

## 🛩️ 耀坤 S3 适配版（`yaokun-organized` 分支）

> ⚠️ **只在 `yaokun-organized` 分支上生效。** `master` 始终跟随上游，不包含以下改动。

这一分支把 Flix 跑到了**耀坤 S3 无人机**上，并解决了"换用自研 ESP-NOW 遥控后飞机毫无反应"的问题。  
改动清单、根因分析与逐条验证记录见 [Docs/耀坤S3适配说明.md](Docs/耀坤S3适配说明.md)。

### 关键改动一览

| 编号      | 改动                                                           | 为什么必须改                                                                                                                                               |
| ------- | ------------------------------------------------------------ | ---------------------------------------------------------------------------------------------------------------------------------------------------- |
| **R1**  | 时间基准改 **64 位微秒累加器**                                          | 上游 `t = micros()/1e6`，而 `micros()` 是 32 位 → **每 71.6 分钟回绕**，`t` 倒退会让全工程 `t - last >= 周期` 的判据集体失效，表现为"上电 71.6 分钟后遥测全停、终端还在回显、reboot 就恢复"              |
| **R2**  | `COMMAND_LONG` **统一回 ACK**                                   | 解锁被拒/模式非法时若不回 ACK，QGC 会一直卡在"正在执行"直到超时                                                                                                                |
| **R4**  | ESP-NOW 接收改用 **原生 `esp_now_register_recv_cb` + 字节队列**        | 上游走 `ESPNOWSerial::read()` 的**会话机制**，只交出已登记 peer 的数据；而登记 peer 靠 4 字节 `flix` 发现包，上游还把它门控在 `isEncrypted()` 里 → **不加密就永远建不了会话，遥控器显示已连接但飞机不动**。这是本次改造的核心 |
| **R5**  | 电机引脚 **15 / 5 / 6 / 16**，PWM 20 kHz                          | 耀坤 S3 实物布线；20 kHz 超出人耳范围，电机不尖叫                                                                                                                       |
| **R6**  | 电压检测 GPIO **2**，分压系数 **2**                                   | 对应耀坤 S3 分压电阻实测值                                                                                                                                      |
| **R7**  | IMU **上电自动识别**（I2C 的 MPU6050 / SPI 的 MPU-6500），朝向按总线分档       | 两块板子用的 IMU 不同，型号选错 `IMU::create()` 返回 `nullptr` → 上游会**空指针崩溃**。本版加了 `imuOK` 判据，IMU 不可用时禁止解锁并红灯长鸣告警                                                   |
| **R8**  | 三色灯（GPIO 41/7/14）+ 蜂鸣器（GPIO 39）灯语系统                          | 现场不看串口也知道状态；**连接判定改用 `controlTime` 而非 `mavlinkTime`** —— 后者任何 MAVLink 帧都会刷新，会"没接遥控也显示已连接"                                                            |
| **R9**  | **全中文界面**（`i18n.h` 集中管理，135 条文案）                             | help 菜单、IMU 校准、初始化提示、错误信息全中文                                                                                                                         |
| **R10** | 参数表新增 `IMU_AUTO_DETECT` / `IMU_ROT_LOCK` / `LED_*` / `BUZ_*` | 换 IMU、换板不必重编固件                                                                                                                                       |

### 中文 / 英文双版本

界面文字全部集中在 `Firmware/flix/i18n.h`，**默认中文**：

```cpp
// 编英文版：在 flix.ino 最顶部（第一条 #include 之前）加这一行
#define FLIX_LANG_EN
```

或者编译时传宏，不改任何文件：

```bash
arduino-cli compile --build-property "compiler.cpp.extra_flags=-DFLIX_LANG_EN" ...
```

两种版本的中英文案条数严格一致（135 : 135），有脚本可校验，不会出现"改了中文忘了英文"。

### 编译与烧录（耀坤 S3）

```bash
cd Firmware
arduino-cli compile \
  --fqbn "esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=4M,PartitionScheme=default,PSRAM=disabled" \
  --libraries "G:\Arduino\libraries" \
  --export-binaries flix
```

> 🔴 **必须带 `--export-binaries`**，否则 `build/` 目录不会更新，打包脚本会拿旧固件拼出一个"看起来正常但内容错误"的 bin。

打包单文件固件 + 自动生成指纹说明：

```bash
python Firmware/tools/pack_flix_release.py --version 1.0
```

产物在 `Firmware/release/`：`*_merged.bin`（烧 `0x0`）+ `*_指纹与烧录说明.md/.txt`。

### 控制台命令（中文版实际显示）

连上串口（115200，**原生 USB 口**）敲 `help`：

```text
可用命令：

help - 显示本帮助
p - 显示全部参数
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
wifi ap/sta/espnow/off - 设置无线模式
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

> 📌 **如果只看到品牌横幅、看不到上面这份命令表**：那是 v1.0 早期构建的截断问题
> （HWCDC 发送缓冲默认仅 256 B，1.7 KB 的菜单一次写出会被静默丢弃）。
> 已修复：`setupConsole()` 把 TX 缓冲扩到 4 KB + 命令收尾补推一次。
> 请按《指纹与烧录说明》核对固件 SHA-256 后重新烧录。

### ESP-NOW 遥控的四个必查项

飞机端和遥控器端**必须**满足以下四点，否则现象一律是"遥控器显示已连接、飞机毫无反应"：

1. **信道一致** —— 飞机端 `ch` 命令查看，默认 **1**。耀坤中文版遥控（`ESP32-DJI-Dev-Yaokun20260815`）**不调用 `esp_wifi_set_channel`**，走 Wi-Fi 默认信道 **1**，所以飞机端也取 1。
   > ⚠️ 早期版本按英文版遥控的 `esp_wifi_set_channel(6)` 把默认值定成 6，结果飞机听 6、遥控发 1，物理层彻底错开。**这个故障极具迷惑性：遥控器"对端发现"页仍能列出飞机**（扫描是跨信道的），"能看到"≠"能通"。
2. **飞机已登记遥控器为 ESP-NOW peer** —— ESP-NOW 的接收回调只对**已登记在 peer 表里**的 MAC 触发，未登记的包在驱动层就被丢弃。中文版遥控 `connect(mac)` 后走**单播**，所以必须登记。本版已实现：开机预登记广播地址 + `onReceive` 记录来源 MAC + 主循环 `learnPeerIfNeeded()` 自动学习（**回调里不能直接 `esp_now_add_peer`，会取 Wi-Fi 锁死锁**）。
3. **遥控器已使能** —— 中文版遥控的 `enabled` 默认是 `false`，此时 `MANUAL_CONTROL` 照发不误，但四轴数值全是中位（≈0.01）。**使能方法 = 按遥控器的「左按钮（辅助按钮）」**，界面上没有这个开关，是纯物理按键。
   > ⚠️ **飞机每次重启，都要重新走一遍：对端发现 → 点选飞机 → 按左按钮使能。**
4. **接收链路已注册** —— 开机日志出现 `ESP-NOW 原生接收已启用`；`wifi` 命令里 `接收丢包` 与 `队列字节` 两项可判断是"发不出去"还是"收不过来"。

**用 `wifi` 命令按这个顺序排查：**

| 现象 | 结论 |
| --- | --- |
| `接收包数` = 0 | 物理层不通 → 查**信道**（第 1 项） |
| 包数涨，`已解析 MAVLink 帧数` = 0 | 协议不匹配 |
| 帧数涨，`最近一帧消息 ID` ≠ 69 | 遥控没发 `MANUAL_CONTROL` |
| msgid = 69，但 `rc` 四轴全 0.01 | **遥控器未使能** → 按左按钮（第 3 项） |
| 全对，四轴跟摇杆动 | ✅ 链路正常 |

> ⚠️ 家庭路由器与飞机热点**同信道**时会互相干扰，表现为手机连上就掉、界面假死。  
> 用 `ch <1-13>` 改飞机端，但**遥控器端必须同步改**，否则物理层直接收不到。

### 飞行能力边界（务必先读）

本固件是**纯姿态自稳 + 手动油门**，没有任何位置/速度传感器（无 GPS / 气压计 / 测距 / 光流）：

- **松杆不会悬停** —— 姿态会稳平，但会保持水平速度继续飘。
- **松油门就掉高度** —— 没有定高环。
- **四源全静默会被判失联** —— 遥控、地面站全断 1 秒后自动降油门，10 秒归零并上锁。

首次试飞请在空旷场地、低空、装保护圈的前提下进行。

## 🚀 快速开始

### 1. 编译固件

需要先安装 [arduino-cli](https://arduino.github.io/arduino-cli/latest/installation/)。

```bash
git clone https://github.com/YaoKunS/flix.git
cd flix
git checkout yaokun-organized   # ★ 切到耀坤整理版分支（默认 master 是上游原版结构）

# 首次运行会自动下载 ESP32 core 3.3.10 与依赖库（FlixPeriph / MAVLink）
make                                    # 默认编译 ESP32
make BOARD=esp32:esp32:esp32s3          # 编译 ESP32-S3
make BOARD=esp32:esp32:esp32c3          # 编译 ESP32-C3
```

> 💡 `make` 在根目录可用 —— 根目录的 `Makefile` 会自动转发到 `Firmware/`。  
> 也可以 `cd Firmware && make`，效果完全一样。

### 2. 烧录与调试

```bash
make upload                 # 编译并烧录
make monitor                # 打开串口监视器（115200）
make erase                  # 擦除 bootloader
```

### 3. 运行 Gazebo 仿真

不想买硬件也能先玩起来（需要 Gazebo 11）：

```bash
make build_simulator
make simulator
```

### 4. 无线遥控

编译烧录 `ESP-NOW` 代理固件：

```bash
make upload_proxy
```

连上飞机的 Wi-Fi（AP 名称见串口日志），即可用浏览器或地面站操控。

#### 飞机热点名规则（耀坤版）

| 项 | 规则 |
| --- | --- |
| 前缀 | **`YaoKun_`**（品牌统一，注意大小写：`Y`、`K` 大写） |
| 后缀 | 芯片 AP 模式 MAC 末两字节（4 位十六进制，大写） |
| 默认密码 | `12345678` |
| 实际值怎么看 | 开机日志里有一行 **`当前热点名：YaoKun_xxxx`**，以它为准 |
| 想改名 | `ap <新热点名> <密码>` 然后 `reboot`；改名后前缀不再自动生成 |

> ⚠️ **别拿 `wifi` 命令打印的「本机 MAC」去推热点名**。
> ESP32 的 AP MAC 和 STA MAC 在第一个字节的第 1 位上不同，
> 1 号机实测：`wifi` 显示 `80:65:99:DF:F2:24`（STA，ESP-NOW 用的是它），
> 而热点名是 **`YaoKun_6202`**（AP MAC 末两字节），不是直觉上的 `YaoKun_F224`。
>
> 📌 **ESP-NOW 模式下也会广播这个热点**（ESP-NOW 只能跑在 AP/STA 接口上，
> `WiFi.mode(WIFI_AP)` 一起就会起热点）。旧版这里用的是 core 默认名 `ESP_xxxxxx`，
> 现场分不清是谁家的机器，现已统一成 `YaoKun_` 前缀。


## 🔌 硬件清单

| Type             | Part                                                                                                                                                                                                |                                                                         Image                                                                         | Quantity |
| ---------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :---------------------------------------------------------------------------------------------------------------------------------------------------: | :------: |
| 主控板              | ESP32 Mini<br />也支持 ESP32-S3 / ESP32-C3                                                                                                                                                             |                                                        <img src="Docs/img/esp32.jpg" width=100>                                                       |     1    |
| IMU¹             | GY-91、MPU-9265（或其它 MPU-9250 / MPU-6500）<br />ICM20948V2<br />GY-521（MPU-6050）                                                                                                                       | <img src="Docs/img/gy-91.jpg" width=90 align=center><br /><img src="Docs/img/icm-20948.jpg" width=100><br /><img src="Docs/img/gy-521.jpg" width=100> |     1    |
| *升压模块（可选，供电更稳）*  | *5V 输出*                                                                                                                                                                                             |                                                     <img src="Docs/img/buck-boost.jpg" width=100>                                                     |     1    |
| 电机               | 8520 3.7V 有刷电机<br />**必须是标称 3.7V，不能是工作电压区间（3.7V–6V）**<br />确认电机轴径与桨孔径匹配！                                                                                                                            |                                                       <img src="Docs/img/motor.jpeg" width=100>                                                       |     4    |
| 桨叶               | 55 mm 或 65 mm                                                                                                                                                                                       |                                                        <img src="Docs/img/prop.jpg" width=100>                                                        |     4    |
| MOSFET           | UMW 100N03A 或[类似型号](https://t.me/opensourcequadcopter/33)<br />⚠️ **不要用 KIA 100N03A 等杂牌，可能不工作！**                                                                                                    |                                                       <img src="Docs/img/100n03a.jpg" width=100>                                                      |     4    |
| 下拉电阻<br />电压采样电阻 | 10 kΩ                                                                                                                                                                                               |                                                     <img src="Docs/img/resistor10k.jpg" width=100>                                                    |     6    |
| 3.7V 锂电池         | LW 952540（或同尺寸兼容型号）<br />建议 25C 1000 mAh 以上！                                                                                                                                                        |                                                       <img src="Docs/img/battery.jpg" width=100>                                                      |     1    |
| 电池线              | MX2.0 2P 母座                                                                                                                                                                                         |                                                         <img src="Docs/img/mx.png" width=100>                                                         |     1    |
| 锂电池充电器           | 任意                                                                                                                                                                                                  |                                                       <img src="Docs/img/charger.jpg" width=100>                                                      |     1    |
| IMU 板固定螺钉        | M3x5                                                                                                                                                                                                |                                                      <img src="Docs/img/screw-m3.jpg" width=100>                                                      |     2    |
| 机架装配螺钉           | M1.4x5                                                                                                                                                                                              |                                               <img src="Docs/img/screw-m1.4.jpg" height=30 align=center>                                              |     4    |
| 机架主体             | 3D 打印²：[`stl`](Docs/assets/flix-frame-1.1.stl) [`step`](Docs/assets/flix-frame-1.1.step)<br />推荐参数：层高 0.2 mm，线宽 0.4 mm，填充 100%。                                                                     |                                                       <img src="Docs/img/frame1.jpg" width=100>                                                       |     1    |
| 机架上盖             | 3D 打印：[`stl`](Docs/assets/esp32-holder.stl) [`step`](Docs/assets/esp32-holder.step)                                                                                                                 |                                                    <img src="Docs/img/esp32-holder.jpg" width=100>                                                    |     1    |
| IMU 板垫片          | 3D 打印：[`stl`](Docs/assets/washer-m3.stl) [`step`](Docs/assets/washer-m3.step)                                                                                                                       |                                                      <img src="Docs/img/washer-m3.jpg" width=100>                                                     |     2    |
| 遥控器（推荐）          | CC2500 发射器，如 BetaFPV LiteRadio CC2500（RC 接收 + Wi-Fi）<br />双摇杆手柄（仅 Wi-Fi）——见[推荐手柄列表](https://docs.qgroundcontrol.com/master/en/qgc-user-guide/setup_view/joystick.html#supported-joysticks)<br />其它⁵ |                                  <img src="Docs/img/betafpv.jpg" width=100><img src="Docs/img/logitech.jpg" width=80>                                 |     1    |
| *RC 接收机（可选）*     | *DF500 或其它³*                                                                                                                                                                                        |                                                         <img src="Docs/img/rx.jpg" width=100>                                                         |     1    |
| 导线               | 推荐 28 AWG                                                                                                                                                                                           |                                                     <img src="Docs/img/wire-28awg.jpg" width=100>                                                     |          |
| 胶带、双面胶           |                                                                                                                                                                                                     |                                                                                                                                                       |          |

*¹ — 目前气压计未使用。*

*² — 此机架为 GY-91 板优化，换其它板需修改固定孔位。*

*³ — 也可用任何 SBUS 接口的发射机 / 接收机组合。*

组装所需工具：3D 打印机、烙铁、助焊焊锡、螺丝刀、万用表。

欢迎自由修改设计与代码，做出你自己的改进版本。  
成果可发到[官方 Telegram 群](https://t.me/opensourcequadcopterchat)，  
或直接联系作者（[E-mail](mailto:okalachev@gmail.com) · [Telegram](https://t.me/okalachev)）。

## 🔧 接线图

### 简化连接图

![Flix version 1 接线图](Docs/img/schematics1.svg)

*（虚线框内为可选元件。）*

电机连接方案：

![MOSFET 接线方案](Docs/img/mosfet-connection.png)

社区贡献的[完整电路图变体](https://miro.com/app/board/uXjVN-dTjoo=/?moveToWidget=3458764612338222067\&cot=14)。

### 接线要点

- 用锂电池的 VCC（+）与 GND（−）给 ESP32 Mini 供电。
- IMU 板通过 VSPI 接入 ESP32 Mini，供电用 3.3V 与 GND：
  | IMU 引脚       | ESP32 引脚     |
  | ------------ | ------------ |
  | GND          | GND          |
  | 3.3V         | 3.3V         |
  | SCL *(SCK)*  | SVP (GPIO18) |
  | SDA *(MOSI)* | GPIO23       |
  | SAO *(MISO)* | GPIO19       |
  | NCS          | GPIO5        |
- MOSFET 必须焊接下拉电阻。
- 电机经 MOSFET 接 ESP32 Mini：
  | 电机                                          | 位置 | 转向  | 桨型 | 电机线   | GPIO           |
  | ------------------------------------------- | -- | --- | -- | ----- | -------------- |
  | Motor 0                                     | 左后 | 逆时针 | B  | 黑 & 白 | GPIO12 *(TDI)* |
  | Motor 1                                     | 右后 | 顺时针 | A  | 蓝 & 红 | GPIO13 *(TCK)* |
  | Motor 2                                     | 右前 | 逆时针 | B  | 黑 & 白 | GPIO14 *(TMS)* |
  | Motor 3                                     | 左前 | 顺时针 | A  | 蓝 & 红 | GPIO15 *(TD0)* |
  | 顺时针电机配蓝红线，对应 A 型桨（桨上有标记）；逆时针电机配黑白线，对应 B 型桨。 |    |     |    |       |                |
- RC 接收机可选接入 ESP32 的 UART2：
  | 接收机引脚       | ESP32 引脚             |
  | ----------- | -------------------- |
  | GND         | GND                  |
  | VIN         | VCC（按接收机而定，也可用 3.3V） |
  | Signal (TX) | GPIO4                |
- 电池电压分压采样可选接入任意 ADC1 引脚（如 ESP32 的 *GPIO32*、ESP32-S3 的 *GPIO3*）。

  ESP32 与 ESP32-S3 [最多可测](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/adc.html#analogsetattenuation) 3.1 V，  
  ESP32-S3 / ESP32-C3 最多 2.5 V，请据此选择分压电阻。

## 📚 推荐阅读顺序

刚接触飞控的话，**按这个顺序读，半天就能搞懂原理**：

1. [固件架构总览](Docs/firmware.md) —— 数据怎么流动、控制怎么闭环
2. [组装说明](Docs/assembly.md) —— 把机器装起来
3. [使用手册](Docs/usage.md) —— 编译、调试、飞行
4. [飞控教材](Docs/book/) —— 姿态解算、坐标系、陀螺仪原理
5. [故障排查](Docs/troubleshooting.md) —— 起飞异常时先看这里
6. [日志分析](Docs/log.md) —— 用 PlotJuggler 分析飞行数据

更多：

- [仿真器说明](Firmware/gazebo/README.md)
- [Python 库文档](Tools/pyflix/README.md)
- [社区作品展](Docs/user.md)

## 🔌 社区资源

- 开发频道（俄语）：<https://t.me/opensourcequadcopter>
- 官方 Telegram 群（英/俄）：<https://t.me/opensourcequadcopterchat>
- 开发历程长文（俄语）：<https://habr.com/ru/articles/814127/>
- 飞控教材网站：<https://quadcopter.dev>

## 📜 开源协议与致谢

- **上游项目**：Flix © [Oleg Kalachev](https://github.com/okalachev) ·  
  源码仓库 <https://github.com/okalachev/flix>
- **本整理版**：仅做中文文档与目录整理，**未修改固件逻辑**，固件版权归原作者所有。
- **本仓库整理**：耀坤智联 YaoKunSmart · <https://github.com/YaoKunS>
- **B 站**：<https://space.bilibili.com/3707011699050767>

> ⚠️ **关于许可证的重要说明**：

> 上游 [okalachev/flix](https://github.com/okalachev/flix) 仓库**目前未附带任何 LICENSE 文件**，  
> 也未在 README 中声明具体开源许可证名称。这意味着严格来说，  
> **版权默认保留给原作者**（著作权法默认「未授权」）。  
> 因此本整理版**不擅自添加** LICENSE 文件，也不会替作者选择许可协议 ——  
> 这属于原作者的决定。  
> 本仓库仅作学习研究与技术交流用途；如需商业使用或再分发，  
> 请先联系原作者取得明确授权，并遵守所在地法律法规。

> ⚠️ 使用、修改、分发本项目时，请保留上游作者署名与项目出处。

## ⚠️ 免责声明

这是一个 DIY（自己动手）项目，希望你觉得它有趣且有用。  
但它并不容易组装和调试，且**按"现状"提供，不附带任何担保** ——  
不保证它能完美工作，甚至不保证它能工作。

⚠️ 作者不对使用本项目造成的任何损坏、伤害或损失负责。**风险自负！**  
飞行时请务必在空旷场地试飞，并遵守当地无人机法规。

---

<div align="center">

**🚁 Flix** · 由 [Oleg Kalachev](https://github.com/okalachev) 倾力打造  
· 本中文整理版由 [耀坤智联](https://github.com/YaoKunS) 维护

</div>
