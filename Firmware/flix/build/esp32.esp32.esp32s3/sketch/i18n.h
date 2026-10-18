#line 1 "D:\\YaoKunS\\Forks\\flix\\Firmware\\flix\\i18n.h"
// Copyright (c) 2026 耀坤智联 / YaoKunSmart <espnow@163.com>
// Repository: https://github.com/YaoKunS/flix
// 上游原作者：(C) Oleg Kalachev <okalachev@gmail.com> / https://github.com/okalachev/flix
//
// ============================================================================
// i18n.h —— 耀坤整理版固件的中英文案表（**界面文字的唯一数据源**）
// ============================================================================
//
// 【为什么单独开一个文件，而不是把中文直接分散写进各个 .ino】
//   1) 用户要求"耀坤整理版界面全中文"，但同时要保留一条能编出英文固件的路
//      （GPL 开源仓给海外用户用）。若中文写死在 30 多个 print() 里，
//      想切英文就得逐行改代码 —— 极易漏改、且 diff 噪音巨大。
//   2) 文案集中在这里之后，"检查界面是否还有英文残留"变成一件**可脚本化**的事：
//      只要确认所有 print() 都走 TXT_* 宏，就不可能出现漏翻。
//   3) 串口屏/网页等其它前端将来要复用同一套文案，也只需要包含本文件。
//
// 【怎么用】
//   默认就是中文（用户指定），无需任何宏。
//   要编英文版：在 flix.ino **最顶部**（第一行 #include 之前）加
//       #define FLIX_LANG_EN
//   或者编译时加 -DFLIX_LANG_EN。
//
// 【约定】
//   - 宏展开后是**字符串字面量**，所以写法必须是 print(TXT_XXX)，不要写成
//     print("%s", TXT_XXX)（能跑但多一次格式化，没必要）。
//   - 带格式参数的提示（如 "油门 %d"）一律拆成"前缀宏 + 格式串"，例如
//        print(TXT_PWM_SET " %d\n", freq);
//     这样中英文各自保持自然的语序，避免"参数位置随语言变"的经典麻烦。
//   - 本文件只放**界面文案**，不放任何逻辑；不含 Unicode 转义风险（UTF-8 保存）。
//
// 【编码注意】
//   本文件必须存为 **UTF-8 无 BOM**。arduino-cli 默认按 UTF-8 解析源文件；
//   若被记事本存成 GBK，中文会变成乱码字节直接发到串口。
// ============================================================================

#pragma once

#ifdef FLIX_LANG_EN
// ============================ 英文版（上游原味） ============================

// ---- 开机 / 通用 ----
#define TXT_BOOT_START          "Initializing Flix\n"
#define TXT_BOOT_VERSION        "Firmware version: v1.0 (YaoKun organized edition)\n"
#define TXT_BOOT_DONE           "Initializing complete\n"
#define TXT_SETUP_PARAMS        "Setup parameters\n"
#define TXT_SETUP_POWER         "Setup power\n"
#define TXT_SETUP_LED           "Setup LED\n"
#define TXT_SETUP_MOTORS        "Setup motors\n"
#define TXT_SETUP_WIFI          "Setup Wi-Fi\n"
#define TXT_SETUP_IMU           "Setup IMU\n"
#define TXT_SETUP_RC            "Setup RC\n"
#define TXT_YES                 "yes"
#define TXT_NO                  "no"

// ---- 控制台通用 ----
#define TXT_INVALID_CMD         "Invalid command: %s\n"
#define TXT_HINT_HELP           "Type 'help' for the command list.\n"
#define TXT_PARAM_NOT_FOUND     "Parameter not found: %s\n"
#define TXT_PARAM_HEADER        "Name             Value          [Default]\n"

// ---- help 菜单（逐行） ----
#define TXT_HELP_TITLE          "Commands:\n\n"
#define TXT_HELP_HELP           "help - show help\n"
#define TXT_HELP_P              "p - show all parameters\n"
#define TXT_HELP_P_STR          "p <str> - show parameters starting with str\n"
#define TXT_HELP_P_SET          "p <name> <value> - set parameter\n"
#define TXT_HELP_PRESET         "preset - reset parameters to defaults (reboot)\n"
#define TXT_HELP_TIME           "time - show time info\n"
#define TXT_HELP_IMU            "imu - show IMU data\n"
#define TXT_HELP_CA             "ca - calibrate accelerometer\n"
#define TXT_HELP_ST             "st - show attitude estimation\n"
#define TXT_HELP_ARM            "arm / disarm - arm or disarm the drone\n"
#define TXT_HELP_MODE           "raw/stab/acro/auto - set flight mode\n"
#define TXT_HELP_RC             "rc - show RC (ESP-NOW) data\n"
#define TXT_HELP_CR             "cr - calibrate RC\n"
#define TXT_HELP_PW             "pw - show power info\n"
#define TXT_HELP_WIFI           "wifi - show Wi-Fi info\n"
#define TXT_HELP_WIFI_SET       "wifi ap/sta/espnow/off - set Wi-Fi mode\n"
#define TXT_HELP_AP             "ap <ssid> <password> - configure access point\n"
#define TXT_HELP_STA            "sta <ssid> <password> - configure Wi-Fi client\n"
#define TXT_HELP_ESPNOW         "espnow <mac> [<key>] - configure ESP-NOW peer\n"
#define TXT_HELP_CH             "ch [<n>] - show or set ESP-NOW channel\n"
#define TXT_HELP_MOT            "mot - show motor output\n"
#define TXT_HELP_LOG            "log [dump] - print log header [and data]\n"
#define TXT_HELP_MFR            "mfr/mfl/mrr/mrl [<thrust>] - test motor (REMOVE PROPS!)\n"
#define TXT_HELP_SYS            "sys - show system info\n"
#define TXT_HELP_RESET          "reset - reset attitude state\n"
#define TXT_HELP_REBOOT         "reboot - reboot the drone\n"

// ---- time ----
#define TXT_TIME                "Time: %f\n"
#define TXT_LOOP_RATE           "Loop rate: %.0f\n"
#define TXT_DT                  "dt: %f\n"
#define TXT_TIMEBASE            "micros() wrap count: %u (harmless, absorbed)\n"

// ---- imu ----
#define TXT_IMU_STATUS_ERR      "status: ERROR %d\n"
#define TXT_IMU_STATUS_OK       "status: OK\n"
#define TXT_IMU_MODEL           "model: %s\n"
#define TXT_IMU_WHOAMI          "who am I: 0x%02X\n"
#define TXT_IMU_RATE            "rate: %.0f\n"
#define TXT_IMU_INT_MODE        "interrupt mode: %s\n"
#define TXT_IMU_TEMP            "temperature: %.1f C\n"
#define TXT_IMU_GYRO            "gyro: %f %f %f\n"
#define TXT_IMU_ACC             "acc: %f %f %f\n"
#define TXT_IMU_RAW_GYRO        "raw gyro: %f %f %f\n"
#define TXT_IMU_RAW_ACC         "raw acc: %f %f %f\n"
#define TXT_IMU_PIN_STR         "pin"
#define TXT_IMU_PIN_TIMER       "timer"
#define TXT_CAL_START           "Calibrating accelerometer\n"
#define TXT_CAL_1               "1/6 Place level [8 sec]\n"
#define TXT_CAL_2               "2/6 Place nose up [8 sec]\n"
#define TXT_CAL_3               "3/6 Place nose down [8 sec]\n"
#define TXT_CAL_4               "4/6 Place on right side [8 sec]\n"
#define TXT_CAL_5               "5/6 Place on left side [8 sec]\n"
#define TXT_CAL_6               "6/6 Place upside down [8 sec]\n"
#define TXT_CAL_GYRO_BIAS       "gyro bias: %f %f %f\n"
#define TXT_CAL_ACC_BIAS        "accel bias: %f %f %f\n"
#define TXT_CAL_ACC_SCALE       "accel scale: %f %f %f\n"
#define TXT_CAL_DONE            "Calibration done!\n"
#define TXT_IMU_LANDED          "landed: %d\n"
#define TXT_IMU_FOUND_I2C       "  Found MPU6050 family (I2C) SDA=%d SCL=%d\n"
#define TXT_IMU_FOUND_SPI       "  Found MPU6500 family (SPI) SCK=%d MISO=%d MOSI=%d CS=%d\n"
#define TXT_IMU_WHOAMI2         "  WHO_AM_I = 0x%02X\n"
#define TXT_IMU_ROT_APPLIED     "  Mounting rotation: Z %+.4f rad  [%s]\n"
#define TXT_IMU_DETECTING       "Detecting IMU...\n"
#define TXT_IMU_PROBE_CS        "  Probing SPI CS=%d, read 0x%02X\n"
#define TXT_IMU_NOT_FOUND       "  IMU not found on I2C nor SPI\n"
#define TXT_IMU_RETRY           "First init failed (model %d / bus %d / status %d), retrying with another model\n"
#define TXT_IMU_RETRY_RESULT    "Retry result: model %d / bus %d / status %d\n"
#define TXT_IMU_UNUSABLE        "IMU unavailable - arming disabled (check socket / wiring)\n"
#define TXT_IMU_READY           "IMU ready: %s, bus %s\n"
#define TXT_IMU_CAL_UNAVAIL     "IMU unavailable, calibration skipped\n"
#define TXT_IMU_NOT_FOUND_SHORT "IMU: not found\n"
#define TXT_IMU_SUMMARY         "Bus: %s  Basis: %s  Available: %d\n"
#define TXT_IMU_PINS_SPI        "Pins: SCK=%d MISO=%d MOSI=%d CS=%d INT=%d\n"
#define TXT_IMU_PINS_I2C        "Pins: SDA=%d SCL=%d INT=%d\n"

// ---- state ----
#define TXT_ST_RATES            "rates: %g %g %g\n"
#define TXT_ST_ATT              "attitude: %g %g %g %g\n"
#define TXT_ST_EULER            "roll: %g deg  pitch: %g deg  yaw: %g deg\n"
#define TXT_ST_LANDED           "landed: %d\n"

// ---- rc ----
#define TXT_RC_CHANNELS         "channels: "
#define TXT_RC_CONTROLS         "\nroll: %g pitch: %g yaw: %g throttle: %g mode: %g\n"
#define TXT_RC_TIME             "time: %.1f\n"
#define TXT_RC_MODE             "mode: %s\n"
#define TXT_RC_ARMED            "armed: %d\n"
#define TXT_RC_DISABLED         "RC_RX_PIN = %d, set the RC pin!\n"
#define TXT_CR_1                "1/8 Calibrating RC: put all switches to default positions [3 sec]\n"
#define TXT_CR_2                "2/8 Move sticks [3 sec]\n...     ...\n...     .o.\n.o.     ...\n"
#define TXT_CR_3                "3/8 Move sticks [3 sec]\n.o.     ...\n...     .o.\n...     ...\n"
#define TXT_CR_4                "4/8 Move sticks [3 sec]\n...     ...\n.o.     .o.\n...     ...\n"
#define TXT_CR_5                "5/8 Move sticks [3 sec]\n...     ...\n..o     .o.\n...     ...\n"
#define TXT_CR_6                "6/8 Move sticks [3 sec]\n...     .o.\n...     ...\n.o.     ...\n"
#define TXT_CR_7                "7/8 Move sticks [3 sec]\n...     ...\n...     ..o\n.o.     ...\n"
#define TXT_CR_8                "8/8 Put mode switch to max [3 sec]\n"
#define TXT_CR_HEADER           "Control   Ch     Zero   Max\n"
#define TXT_CR_SUMMARY          "%d/%d controls calibrated\n"

// ---- power ----
#define TXT_VOLTAGE             "Voltage: %.2f V\n"
#define TXT_VOLT_PIN_ERR        "Voltage pin %d is not an ADC pin, disabled\n"
#define TXT_VOLT_PIN_OK         "Voltage sense: GPIO%d, divider x%g\n"

// ---- wifi ----
#define TXT_WIFI_MODE_ESPNOW    "Mode: ESP-NOW\n"
#define TXT_WIFI_VERSION        "ESP-NOW version: %d\n"
#define TXT_WIFI_MAXLEN         "Max packet size: %d\n"
#define TXT_WIFI_MAC            "MAC: %s\n"
#define TXT_WIFI_PEER           "Peer MAC: %s\n"
#define TXT_WIFI_ENCRYPTED      "Encrypted: %d\n"
#define TXT_WIFI_CHANNEL        "Channel: %d\n"
#define TXT_WIFI_LOST           "Lost packets: %d\n"
#define TXT_WIFI_QBYTES         "Queue pending bytes: %d\n"
#define TXT_WIFI_QFAIL          "Failed to create RX queue\n"
#define TXT_WIFI_CBFAIL         "Failed to register ESP-NOW RX callback\n"
#define TXT_RC_SBUS_OFF         "RC: SBUS disabled (using ESP-NOW), rcRxPin = %d\n"
#define TXT_RC_ESPNOW_NOCAL     "Note: ESP-NOW RC needs no calibration (stick range is +/-1000 on the remote side).\n"
#define TXT_WIFI_NATIVE         "Native recv active: %d\n"
#define TXT_WIFI_RXPACKETS      "RX packets: %u\n"
#define TXT_WIFI_RXBYTES        "RX bytes: %u\n"
#define TXT_WIFI_RXMAC          "Last sender MAC: %02X:%02X:%02X:%02X:%02X:%02X\n"
#define TXT_WIFI_PEER_LEARNED   "Learned new ESP-NOW peer: %02X:%02X:%02X:%02X:%02X:%02X\n"
#define TXT_WIFI_MSGS           "MAVLink frames parsed: %u\n"
#define TXT_WIFI_LASTMSG        "Last MAVLink msgid: %u (MANUAL_CONTROL=69)\n"
#define TXT_WIFI_LASTSYS        "Last MAVLink source sysid: %u\n"
#define TXT_WIFI_AP             "Mode: Access Point (AP)\n"
#define TXT_WIFI_SSID           "SSID: %s\n"
#define TXT_WIFI_PASS_MASK      "Password: ***\n"
#define TXT_WIFI_CLIENTS        "Clients: %d\n"
#define TXT_WIFI_IP             "IP: %s\n"
#define TXT_WIFI_REMOTE_IP      "Remote IP: %s\n"
#define TXT_WIFI_STA            "Mode: Client (STA)\n"
#define TXT_WIFI_CONNECTED      "Connected: %d\n"
#define TXT_WIFI_RSSI           "RSSI: %d dBm\n"
#define TXT_WIFI_OFF            "Mode: Disabled\n"
#define TXT_WIFI_MAVLINK        "MAVLink connected: %d\n"
#define TXT_WIFI_INVALID        "Invalid configuration\n"
#define TXT_WIFI_REBOOT         "Reboot to apply new settings\n"
#define TXT_WIFI_MODE_SET       "Wi-Fi mode set to %s, reboot to apply\n"
#define TXT_WIFI_MODE_INVALID   "Invalid Wi-Fi mode\n"
#define TXT_WIFI_NATIVE_OK      "Native ESP-NOW recv initialized\n"
#define TXT_WIFI_NATIVE_FAIL    "Native ESP-NOW recv init FAILED\n"
#define TXT_CH_SET              "ESP-NOW channel set to %d (remote must match), reboot to apply\n"
#define TXT_CH_RANGE            "ESP-NOW channel must be within 1~13\n"

// ---- motors ----
#define TXT_MOT_OUT             "front-right %g front-left %g rear-right %g rear-left %g\n"
#define TXT_MOT_TEST            "Testing motor %d\n"
#define TXT_MOT_DONE            "Done\n"

// ---- parameters ----
#define TXT_PRESET_DONE         "Parameters reset to defaults, rebooting\n"

// ---- mavlink ----
#define TXT_MAV_DENIED_THROTTLE "Arm denied: throttle is not low\n"
#define TXT_MAV_DENIED_MODE     "Set mode denied: invalid mode\n"

// ---- 系统信息 ----
#define TXT_SYS_CHIP            "Chip: %s\n"
#define TXT_SYS_TEMP            "Temperature: %.1f C\n"
#define TXT_SYS_RAM             "Total RAM: %d KB\n"
#define TXT_SYS_HEAP            "Free heap: %d KB\n"
#define TXT_SYS_FIRMWARE        "Firmware: " __DATE__ " " __TIME__ "\n"
#define TXT_SYS_TASKS           "Num  Task                MinSt  Prio  Core  CPU%%\n"

#else
// ============================ 中文版（默认，耀坤整理版） ====================

// ---- 开机 / 通用 ----
#define TXT_BOOT_START          "耀坤S3 飞控（Flix 整理版）启动中…\n"
#define TXT_BOOT_VERSION        "固件版本：v1.0（耀坤整理版）\n"
#define TXT_BOOT_DONE           "初始化完成\n"
#define TXT_SETUP_PARAMS        "初始化参数存储\n"
#define TXT_SETUP_POWER         "初始化电源检测\n"
#define TXT_SETUP_LED           "初始化指示灯与蜂鸣器\n"
#define TXT_SETUP_MOTORS        "初始化电机输出\n"
#define TXT_SETUP_WIFI          "初始化无线通信\n"
#define TXT_SETUP_IMU           "初始化惯性测量单元\n"
#define TXT_SETUP_RC            "初始化遥控接收\n"
#define TXT_YES                 "是"
#define TXT_NO                  "否"

// ---- 控制台通用 ----
#define TXT_INVALID_CMD         "未知命令：%s\n"
#define TXT_HINT_HELP           "输入 help 查看全部命令。\n"
#define TXT_PARAM_NOT_FOUND     "找不到参数：%s\n"
#define TXT_PARAM_HEADER        "参数名          当前值        [默认值]\n"

// ---- help 菜单（逐行） ----
#define TXT_HELP_TITLE          "可用命令：\n\n"
#define TXT_HELP_HELP           "help - 显示本帮助\n"
#define TXT_HELP_P              "p - 显示全部参数\n"
#define TXT_HELP_P_STR          "p <前缀> - 显示以该前缀开头的参数\n"
#define TXT_HELP_P_SET          "p <参数名> <数值> - 修改参数（自动存盘）\n"
#define TXT_HELP_PRESET         "preset - 恢复出厂参数（会重启）\n"
#define TXT_HELP_TIME           "time - 显示时间与主循环频率\n"
#define TXT_HELP_IMU            "imu - 显示惯性测量单元数据\n"
#define TXT_HELP_CA             "ca - 校准加速度计（六面校准）\n"
#define TXT_HELP_ST             "st - 显示姿态估计结果\n"
#define TXT_HELP_ARM            "arm / disarm - 解锁 / 上锁\n"
#define TXT_HELP_MODE           "raw/stab/acro/auto - 切换飞行模式\n"
#define TXT_HELP_RC             "rc - 显示遥控（ESP-NOW）数据\n"
#define TXT_HELP_CR             "cr - 校准遥控行程\n"
#define TXT_HELP_PW             "pw - 显示电源与电压\n"
#define TXT_HELP_WIFI           "wifi - 显示无线连接信息\n"
#define TXT_HELP_WIFI_SET       "wifi ap/sta/espnow/off - 设置无线模式\n"
#define TXT_HELP_AP             "ap <热点名> <密码> - 设置飞机热点\n"
#define TXT_HELP_STA            "sta <路由名> <密码> - 连接家里的路由器\n"
#define TXT_HELP_ESPNOW         "espnow <遥控器MAC> [<密钥>] - 设置 ESP-NOW 对端\n"
#define TXT_HELP_CH             "ch [<信道>] - 查看或设置 ESP-NOW 信道\n"
#define TXT_HELP_MOT            "mot - 显示四路电机输出\n"
#define TXT_HELP_LOG            "log [dump] - 打印日志表头 [以及数据]\n"
#define TXT_HELP_MFR            "mfr/mfl/mrr/mrl [<油门>] - 单独测试电机（务必拆桨！）\n"
#define TXT_HELP_SYS            "sys - 显示系统信息\n"
#define TXT_HELP_RESET          "reset - 复位姿态估计\n"
#define TXT_HELP_REBOOT         "reboot - 重启飞机\n"

// ---- time ----
#define TXT_TIME                "运行时间：%f 秒\n"
#define TXT_LOOP_RATE           "主循环频率：%.0f Hz\n"
#define TXT_DT                  "单步耗时：%f 秒\n"
#define TXT_TIMEBASE            "微秒计数器回绕次数：%u（已吸收，不影响运行）\n"

// ---- imu ----
#define TXT_IMU_STATUS_ERR      "状态：错误 %d\n"
#define TXT_IMU_STATUS_OK       "状态：正常\n"
#define TXT_IMU_MODEL           "型号：%s\n"
#define TXT_IMU_WHOAMI          "芯片标识（WHO_AM_I）：0x%02X\n"
#define TXT_IMU_RATE            "采样频率：%.0f Hz\n"
#define TXT_IMU_INT_MODE        "中断方式：%s\n"
#define TXT_IMU_TEMP            "芯片温度：%.1f 摄氏度\n"
#define TXT_IMU_GYRO            "角速度：%f %f %f\n"
#define TXT_IMU_ACC             "加速度：%f %f %f\n"
#define TXT_IMU_RAW_GYRO        "角速度原始值：%f %f %f\n"
#define TXT_IMU_RAW_ACC         "加速度原始值：%f %f %f\n"
#define TXT_IMU_PIN_STR         "引脚中断"
#define TXT_IMU_PIN_TIMER       "定时器"
#define TXT_CAL_START           "开始校准加速度计\n"
#define TXT_CAL_1               "第 1/6 步：请水平放置，保持静止 [8 秒]\n"
#define TXT_CAL_2               "第 2/6 步：请机头朝上立起 [8 秒]\n"
#define TXT_CAL_3               "第 3/6 步：请机头朝下倒立 [8 秒]\n"
#define TXT_CAL_4               "第 4/6 步：请右侧朝下侧放 [8 秒]\n"
#define TXT_CAL_5               "第 5/6 步：请左侧朝下侧放 [8 秒]\n"
#define TXT_CAL_6               "第 6/6 步：请底朝天倒置 [8 秒]\n"
#define TXT_CAL_GYRO_BIAS       "陀螺零偏：%f %f %f\n"
#define TXT_CAL_ACC_BIAS        "加速度零偏：%f %f %f\n"
#define TXT_CAL_ACC_SCALE       "加速度比例系数：%f %f %f\n"
#define TXT_CAL_DONE            "校准完成！\n"
#define TXT_IMU_LANDED          "已落地：%d\n"
#define TXT_IMU_FOUND_I2C       "  识别为 MPU6050 系（I2C）SDA=%d SCL=%d\n"
#define TXT_IMU_FOUND_SPI       "  识别为 MPU6500 系（SPI）SCK=%d MISO=%d MOSI=%d CS=%d\n"
#define TXT_IMU_WHOAMI2         "  芯片标识（WHO_AM_I）= 0x%02X\n"
#define TXT_IMU_ROT_APPLIED     "  安装朝向补偿：绕 Z 轴 %+.4f 弧度  [%s]\n"
#define TXT_IMU_DETECTING       "正在自动识别惯性测量单元…\n"
#define TXT_IMU_PROBE_CS        "  试探 SPI 片选 CS=%d，读数 0x%02X\n"
#define TXT_IMU_NOT_FOUND       "  I2C 与 SPI 都没有找到惯性测量单元\n"
#define TXT_IMU_RETRY           "首次初始化失败（型号 %d / 总线 %d / 状态 %d），换型号重试\n"
#define TXT_IMU_RETRY_RESULT    "重试结果：型号 %d / 总线 %d / 状态 %d\n"
#define TXT_IMU_UNUSABLE        "惯性测量单元不可用 —— 已禁止解锁（请检查排母/接线）\n"
#define TXT_IMU_READY           "惯性测量单元就绪：%s，总线 %s\n"
#define TXT_IMU_CAL_UNAVAIL     "惯性测量单元不可用，无法校准\n"
#define TXT_IMU_NOT_FOUND_SHORT "惯性测量单元：未找到\n"
#define TXT_IMU_SUMMARY         "总线：%s　识别依据：%s　可用：%d\n"
#define TXT_IMU_PINS_SPI        "引脚：SCK=%d MISO=%d MOSI=%d CS=%d INT=%d\n"
#define TXT_IMU_PINS_I2C        "引脚：SDA=%d SCL=%d INT=%d\n"

// ---- state ----
#define TXT_ST_RATES            "角速度：%g %g %g\n"
#define TXT_ST_ATT              "四元数：%g %g %g %g\n"
#define TXT_ST_EULER            "横滚：%g 度　俯仰：%g 度　偏航：%g 度\n"
#define TXT_ST_LANDED           "已落地：%d\n"

// ---- rc ----
#define TXT_RC_CHANNELS         "原始通道："
#define TXT_RC_CONTROLS         "\n横滚：%g 俯仰：%g 偏航：%g 油门：%g 模式开关：%g\n"
#define TXT_RC_TIME             "最后收到遥控的时刻：%.1f 秒\n"
#define TXT_RC_MODE             "飞行模式：%s\n"
#define TXT_RC_ARMED            "解锁状态：%d\n"
#define TXT_RC_DISABLED         "遥控引脚 RC_RX_PIN = %d，未启用 SBUS\n"
#define TXT_CR_1                "第 1/8 步：把全部开关拨到默认位置 [3 秒]\n"
#define TXT_CR_2                "第 2/8 步：请推拉左摇杆（上下）[3 秒]\n...     ...\n...     .o.\n.o.     ...\n"
#define TXT_CR_3                "第 3/8 步：请推拉左摇杆（上下）[3 秒]\n.o.     ...\n...     .o.\n...     ...\n"
#define TXT_CR_4                "第 4/8 步：请推拉右摇杆（左右）[3 秒]\n...     ...\n.o.     .o.\n...     ...\n"
#define TXT_CR_5                "第 5/8 步：请推拉左摇杆（左右）[3 秒]\n...     ...\n..o     .o.\n...     ...\n"
#define TXT_CR_6                "第 6/8 步：请推拉右摇杆（上下）[3 秒]\n...     .o.\n...     ...\n.o.     ...\n"
#define TXT_CR_7                "第 7/8 步：请推拉右摇杆（左右）[3 秒]\n...     ...\n...     ..o\n.o.     ...\n"
#define TXT_CR_8                "第 8/8 步：把模式开关拨到最大档 [3 秒]\n"
#define TXT_CR_HEADER           "控制项    通道   零位   满量程\n"
#define TXT_CR_SUMMARY          "共完成 %d/%d 项遥控校准\n"

// ---- power ----
#define TXT_VOLTAGE             "电池电压：%.2f 伏\n"
#define TXT_VOLT_PIN_ERR        "电压检测引脚 %d 不是 ADC 口，已禁用\n"
#define TXT_VOLT_PIN_OK         "电压检测：GPIO%d，分压系数 %g\n"

// ---- wifi ----
#define TXT_WIFI_MODE_ESPNOW    "无线模式：ESP-NOW\n"
#define TXT_WIFI_VERSION        "ESP-NOW 版本：%d\n"
#define TXT_WIFI_MAXLEN         "单包最大长度：%d 字节\n"
#define TXT_WIFI_MAC            "本机 MAC：%s\n"
#define TXT_WIFI_PEER           "对端 MAC：%s\n"
#define TXT_WIFI_ENCRYPTED      "是否加密：%d\n"
#define TXT_WIFI_CHANNEL        "信道：%d\n"
#define TXT_WIFI_LOST           "发送丢包数：%d\n"
#define TXT_WIFI_QBYTES         "接收队列积压：%d 字节\n"
#define TXT_WIFI_QFAIL          "接收队列创建失败\n"
#define TXT_WIFI_CBFAIL         "注册 ESP-NOW 接收回调失败\n"
#define TXT_RC_SBUS_OFF         "遥控：未启用 SBUS（走 ESP-NOW），rcRxPin = %d\n"
#define TXT_RC_ESPNOW_NOCAL     "提示：ESP-NOW 遥控无需校准（摇杆量程由遥控器固件保证 ±1000）。\n"
#define TXT_WIFI_NATIVE         "原生接收通道：%d\n"
#define TXT_WIFI_RXPACKETS      "接收包数：%u\n"
#define TXT_WIFI_RXBYTES        "接收字节数：%u\n"
#define TXT_WIFI_RXMAC          "最近发包方 MAC：%02X:%02X:%02X:%02X:%02X:%02X\n"
#define TXT_WIFI_PEER_LEARNED   "已登记新的 ESP-NOW 对端：%02X:%02X:%02X:%02X:%02X:%02X\n"
#define TXT_WIFI_MSGS           "已解析 MAVLink 帧数：%u\n"
#define TXT_WIFI_LASTMSG        "最近一帧消息 ID：%u（MANUAL_CONTROL=69）\n"
#define TXT_WIFI_LASTSYS        "最近一帧来源 sysid：%u\n"
#define TXT_WIFI_AP             "无线模式：热点（AP）\n"
#define TXT_WIFI_SSID           "热点名称：%s\n"
#define TXT_WIFI_PASS_MASK      "热点密码：***\n"
#define TXT_WIFI_CLIENTS        "已连设备数：%d\n"
#define TXT_WIFI_IP             "本机 IP：%s\n"
#define TXT_WIFI_REMOTE_IP      "对端 IP：%s\n"
#define TXT_WIFI_STA            "无线模式：客户端（STA）\n"
#define TXT_WIFI_CONNECTED      "是否已连接：%d\n"
#define TXT_WIFI_RSSI           "信号强度：%d dBm\n"
#define TXT_WIFI_OFF            "无线模式：已关闭\n"
#define TXT_WIFI_MAVLINK        "MAVLink 链路在线：%d\n"
#define TXT_WIFI_INVALID        "配置无效，请检查参数\n"
#define TXT_WIFI_REBOOT         "已保存，重启后生效\n"
#define TXT_WIFI_MODE_SET       "无线模式已设为 %s，重启后生效\n"
#define TXT_WIFI_MODE_INVALID   "无线模式无效\n"
#define TXT_WIFI_NATIVE_OK      "原生 ESP-NOW 接收已就绪\n"
#define TXT_WIFI_NATIVE_FAIL    "原生 ESP-NOW 接收初始化失败\n"
#define TXT_CH_SET              "ESP-NOW 信道已设为 %d（遥控器必须一致），重启后生效\n"
#define TXT_CH_RANGE            "ESP-NOW 信道必须是 1~13\n"

// ---- motors ----
#define TXT_MOT_OUT             "右前 %g　左前 %g　右后 %g　左后 %g\n"
#define TXT_MOT_TEST            "正在测试电机 %d\n"
#define TXT_MOT_DONE            "测试结束\n"

// ---- parameters ----
#define TXT_PRESET_DONE         "参数已恢复出厂值，正在重启\n"

// ---- mavlink ----
#define TXT_MAV_DENIED_THROTTLE "拒绝解锁：油门不在最低位\n"
#define TXT_MAV_DENIED_MODE     "设置模式被拒绝：模式号无效\n"

// ---- 系统信息 ----
#define TXT_SYS_CHIP            "芯片型号：%s\n"
#define TXT_SYS_TEMP            "芯片温度：%.1f 摄氏度\n"
#define TXT_SYS_RAM             "总内存：%d KB\n"
#define TXT_SYS_HEAP            "剩余内存：%d KB\n"
#define TXT_SYS_FIRMWARE        "编译时间：" __DATE__ " " __TIME__ "\n"
#define TXT_SYS_TASKS           "序号 任务名              最小栈  优先级 核心 CPU%%\n"

#endif // FLIX_LANG_EN
