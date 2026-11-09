// Copyright (c) 2026 YaoKunS
// Repository: https://github.com/YaoKunS/flix
//
// ============================================================================
// 板型引脚表（Board Pin Map）
// ============================================================================
//
// 【这个文件解决什么问题】
//   改造前，引脚硬编码在 7 个位置：
//     motors.ino:23   motorPins[4] = {15, 5, 6, 16}
//     imu.ino:34      imuSckPin/imuMisoPin/imuMosiPin/imuCsPin/imuIntPin
//     imu.ino:37      imuSdaPin / imuSclPin
//     imu.ino:177     csCandidates[3] = { imuCsPin, 42, 17 }   ← 藏在探测逻辑里
//     led.ino:54-57   ledRedPin / ledGreenPin / ledBluePin / buzzerPin
//     power.ino:23    voltagePin
//     rc.ino:25       rcRxPin
//   另外 config.h 里还有一段「按芯片型号覆盖电机脚」的逻辑（经典 ESP32 → 12/13/14/15）。
//   同一份源码要同时支持两种 PCB 时，只要还有一处数字散落在业务代码里，
//   "改一个板型连累另一个"就几乎必然发生。这里把所有引脚收敛成
//   「一个板型 = 一组宏」，业务代码只认 PIN_xxx，不再出现裸数字。
//
// 【🔴 为什么用 #if 宏，而不是运行时查表】
//   硬性要求：耀坤 S3 板（v1.1 已在飞）的编译产物必须与改造前**逐字节一致**，
//   这样"零变化"可以用 sha256 比对 bin 来证明，而不是靠人肉 review 保证。
//   若用运行时查表（如 BOARDS[FLIX_BOARD].motor[0]），编译器会生成查表指令、
//   把结构体放进 RAM，产物必然变化，就无法用二进制比对来证明。
//   用 #if 在预处理阶段就把数值定死，常量折叠的结果与手写数字完全相同。
//
// 【怎么加第三块板】
//   1. 下面加一个板型编号（如 #define BOARD_XXX 3）
//   2. 加一个 #elif FLIX_BOARD == BOARD_XXX 分支，照抄一组 PIN_*
//   3. 编译时加 -DFLIX_BOARD=3（或用 sketch.yaml 的 profile）
//   业务代码（motors/imu/led/power/rc）一行都不用改。
//
// 【怎么验证某块板的引脚真的生效了】
//   串口 `p MOT_PIN_RL` 等命令读回实际值；换板型后必须确认，
//   因为 NVS 里的旧参数会覆盖这里的默认值（见文档 7.7 节）。
// ============================================================================

#pragma once

// ---------------------------------------------------------------------------
// 板型编号
// ---------------------------------------------------------------------------
#define BOARD_YAOKUN_S3     1   // 耀坤 S3 无人机 PCB v2（ESP32-S3，三色 LED，有电压检测）
#define BOARD_HLG_V2        2   // 微辣火龙果 ESP32 迷你无人机 2.0（ESP32 DevKit V1，单 LED）
#define BOARD_GENERIC_ESP32 9   // 上游通用经典 ESP32 默认（只保留历史 motorPins 12/13/14/15）

// ---------------------------------------------------------------------------
// 默认板型：按芯片型号自动选择
// ---------------------------------------------------------------------------
// 【为什么这样默认】保证"编译时不指定任何东西"的行为与改造前**完全一致**：
//   - S3 芯片      → 耀坤 S3 板（改造前唯一的实际目标板）
//   - 经典 ESP32   → 上游通用默认（改造前 config.h 会用 12/13/14/15 覆盖电机脚，
//                    这里用一个独立板型编号把那段历史行为原样保留下来）
// 火龙果板必须显式指定：-DFLIX_BOARD=BOARD_HLG_V2（或 -DFLIX_BOARD=2），
// 绝不会"编到经典 ESP32 就自动变成火龙果" —— 电机引脚没有身份标识，
// 猜错板型的代价是上电四轴乱序起转，所以板型必须由人明确指定。
#ifndef FLIX_BOARD
	#if defined(CONFIG_IDF_TARGET_ESP32S3)
		#define FLIX_BOARD BOARD_YAOKUN_S3
	#elif defined(CONFIG_IDF_TARGET_ESP32)
		#define FLIX_BOARD BOARD_GENERIC_ESP32
	#else
		#define FLIX_BOARD BOARD_YAOKUN_S3
	#endif
#endif

// ===========================================================================
// 板型 1：耀坤 S3 无人机 PCB v2（立创 R8N8 / 淘宝 N16R8）
// ===========================================================================
// 来源：《耀坤S3无人机引脚定义.xlsx》V2 版 + 2026-10 实测。
// 🔴 本块的所有数值必须与改造前逐项一致，否则 sha256 比对会失败。
#if FLIX_BOARD == BOARD_YAOKUN_S3

	#define BOARD_NAME "YaoKun S3 PCB v2"

	// ---- 电机（顺序固定：左后 / 右后 / 右前 / 左前）----
	#define PIN_MOTOR_RL 15
	#define PIN_MOTOR_RR 5
	#define PIN_MOTOR_FR 6
	#define PIN_MOTOR_FL 16

	// ---- IMU SPI（MPU6500）----
	// CS 默认填实测值 17（V1 老板）；V2 新板是 42，靠下面的候选表自动兼容
	#define PIN_IMU_SCK  38
	#define PIN_IMU_MISO 21
	#define PIN_IMU_MOSI 18
	#define PIN_IMU_CS   17
	#define PIN_IMU_INT  -1

	// ---- IMU I2C0（MPU6050 / 磁罗盘 / 气压计）----
	// 🔴 别写成 SDA=8 / SCL=9：曾按错误记忆写反，导致 2 号机 I2C 模块探不到
	#define PIN_IMU_SDA  9
	#define PIN_IMU_SCL  8

	// ---- LED（三色，共阴：高电平点亮）----
	#define PIN_LED_R    41   // 红：低电报警 / 失联慢闪
	#define PIN_LED_G    7    // 绿：已连接常亮 / 已解锁快闪
	#define PIN_LED_B    14   // 蓝：初始化过程指示

	// ---- 其他 ----
	#define PIN_BUZZER   39   // 有源蜂鸣器，高电平触发（S8050 驱动）
	#define PIN_VOLTAGE  2    // 电池分压输入（1S，分压比 2.0）
	// ---- 遥控接收（SBUS）----
	// v1.2 起出厂默认【打开】SBUS，接收脚 = GPIO1（板上 EXT01 的 SBUS 输入）。
	//
	// 【为什么改】v1.1 及以前是 -1（关闭），现场要用 SBUS 得自己 `p RC_RX_PIN 1`。
	//   但对外发布件是给"没有 Arduino IDE、只会烧录"的用户用的 —— 让他们先烧固件
	//   再找串口终端敲参数，等于把这个功能藏起来了。改成默认打开后：
	//   不接 SBUS 时 GPIO1 上没有合法 SBUS 帧（帧头 0x0F + 尾部标志校验，
	//   噪声基本进不来），行为与 v1.1 完全一致；接上就能用。
	//
	// 🔴 只打开引脚还不够：未标定的 SBUS 会让 normalizeRC() 除零得到 inf，
	//   进而 thrustTarget = inf（control.ino:55 没有 clamp）= 满油。
	//   配套的两道保险在 rc.ino 的 decleZero 保护 + config.h 的出厂标定，
	//   三处缺一不可，改这里之前先确认那两处还在。
	//
	// 火龙果 2.0 板本来就是 4（见下面 BOARD_HLG_V2 段），不受本次改动影响。
	#define PIN_RC_RX    1

	// ---- IMU 片选探测候选（兼容 V1/V2 两版 PCB）----
	// V1 老板 MPU6500 片选实测 GPIO17；V2 新板按引脚表是 GPIO42。
	// 上电依次试 {PIN_IMU_CS, ALT1, ALT2}，谁读到合法 WHO_AM_I 就用谁。
	#define PIN_IMU_CS_ALT1 42
	#define PIN_IMU_CS_ALT2 17

	// ---- 能力标记 ----
	#define BOARD_LED_COUNT 3   // 三色灯：红/绿/蓝各自独立
	#define BOARD_HAS_VOLTAGE 1

// ===========================================================================
// 板型 2：微辣火龙果 ESP32 迷你无人机 2.0（ESP32 DevKit V1，30 针）
// ===========================================================================
// 来源：用户 2026-11-05 提供的 PCB 实测引脚。
// 目标：同一份源码，编译期切板型即可产出第二套固件。
// 详细审查（strapping / ADC 分组 / ESP-Drone 预留）见
//   D:\Workbuddy\YaoKunUAV\火龙果2.0适配方案.md
#elif FLIX_BOARD == BOARD_HLG_V2

	#define BOARD_NAME "HLG 2.0 (ESP32 DevKit V1)"

	// ---- 电机 ----
	// 用户给的是顺时针编号 M1~M4，flix 数组顺序是 RL/RR/FR/FL，映射如下：
	//   M3 左后 → RL=26    M2 右后 → RR=27    M1 右前 → FR=33    M4 左前 → FL=32
	// 混控矩阵只认机体位置、不认 GPIO，所以不用改。
	#define PIN_MOTOR_RL 26
	#define PIN_MOTOR_RR 27
	#define PIN_MOTOR_FR 33
	#define PIN_MOTOR_FL 32

	// ---- IMU SPI（MPU6500）----
	// 注意 ESP-Drone 的命名是 MOS/MIS：SDI=MOSI=23，SDO=MISO=19，别搞反
	#define PIN_IMU_SCK  18   // SCLK
	#define PIN_IMU_MISO 19   // SDO
	#define PIN_IMU_MOSI 23   // SDI
	#define PIN_IMU_CS   5    // NCS
	#define PIN_IMU_INT  -1   // 模块未引出中断，靠定时器采样

	// ---- IMU I2C0（第 1 组：MPU6050 / BMP280 / MS5611 / HMC5883 / QMC5883 排母）----
	// 🔴 MPU6500 与 MPU6050 不会同时插，靠 imu.ino 的自动识别区分总线
	#define PIN_IMU_SDA  21
	#define PIN_IMU_SCL  22

	// ---- LED（只有 1 颗板载单色灯，接在 GPIO2）----
	// 🔴 绿/蓝置 -1：led.ino 的合成层检测到 G/B 无效时，会把三个灯的语义
	//    合并到这一颗灯上，用不同闪频区分状态（详见 led.ino 的 ledRender()）。
	// 🔴 接法必须是「GPIO2 → 限流电阻 → LED → GND」（拉电流，高电平点亮）。
	//    若接成灌电流（LED 阳极接 3V3），高低电平会反过来，灯语全部颠倒。
	#define PIN_LED_R    2
	#define PIN_LED_G    -1
	#define PIN_LED_B    -1

	// ---- 其他 ----
	#define PIN_BUZZER   25   // 有源蜂鸣器（用户 2026-11-05 确认）
	// 🔴 电池 ADC 必须落在 ADC1（32/33/34/35/36/39），不能是 ADC2。
	//    ADC2 与 Wi-Fi 互斥，而本固件热点常开 ⇒ 读数恒 0 ⇒ 误判低电 ⇒ 蜂鸣器无限急鸣。
	//    GPIO35 = ADC1_CH7，且是 input-only（不会被误配成输出），是最合适的一个。
	#define PIN_VOLTAGE  35
	#define PIN_RC_RX    4    // SBus 输入

	// ---- IMU 片选探测候选 ----
	// 本板只有一个 SPI 片选（GPIO5），没有兼容候选板；填 -1 表示"不探测"。
	// 🔴 这里绝不能沿用 S3 的 42：经典 ESP32 只有 GPIO0~39，
	//    对 42 号脚做 digitalWrite 会越界写寄存器（未定义行为）。
	#define PIN_IMU_CS_ALT1 -1
	#define PIN_IMU_CS_ALT2 -1

	// ---- 能力标记 ----
	#define BOARD_LED_COUNT 1
	#define BOARD_HAS_VOLTAGE 1

// ===========================================================================
// 板型 9：上游通用经典 ESP32 默认
// ===========================================================================
// 【为什么保留它】改造前 config.h 有一段：
//     #ifdef CONFIG_IDF_TARGET_ESP32
//         motorPins[MOT_RL] = 12; ... = 15;
//     #endif
// 也就是说"编到经典 ESP32 但不指定板型"时，电机脚会被偷偷改成 12/13/14/15。
// 为了不改变这条历史行为（上游兼容），这里用一个独立板型编号把原样结果写死。
// 🔴 本产品线不使用这个板型；它存在的唯一意义是"别人拿这份源码编经典 ESP32 时
//    行为与改造前相同"。
#elif FLIX_BOARD == BOARD_GENERIC_ESP32

	#define BOARD_NAME "Generic ESP32 (upstream default)"

	// 上游经典 ESP32 默认电机脚（改造前由 config.h 覆盖而来）
	#define PIN_MOTOR_RL 12
	#define PIN_MOTOR_RR 13
	#define PIN_MOTOR_FR 14
	#define PIN_MOTOR_FL 15

	// 其余引脚沿用改造前各文件里的默认值（即 S3 那套）。
	// 【说明】这不是"合理配置"，只是"改造前的既有结果"，原样保留以免行为漂移。
	#define PIN_IMU_SCK  38
	#define PIN_IMU_MISO 21
	#define PIN_IMU_MOSI 18
	#define PIN_IMU_CS   17
	#define PIN_IMU_INT  -1
	#define PIN_IMU_SDA  9
	#define PIN_IMU_SCL  8
	#define PIN_LED_R    41
	#define PIN_LED_G    7
	#define PIN_LED_B    14
	#define PIN_BUZZER   39
	#define PIN_VOLTAGE  2
	#define PIN_RC_RX    -1
	#define PIN_IMU_CS_ALT1 42
	#define PIN_IMU_CS_ALT2 17

	#define BOARD_LED_COUNT 3
	#define BOARD_HAS_VOLTAGE 1

#else
	#error "FLIX_BOARD 取值非法：请在 board.h 里选择已定义的板型编号"
#endif

// ---------------------------------------------------------------------------
// 通用行为常量（与板型无关，但要被多个 .ino 共用）
// ---------------------------------------------------------------------------
// 【为什么放这里而不是写在 rc.ino 里】Arduino 把所有 .ino 拼成一个 cpp：
//   flix.ino（主文件）排最前，其余按**文件名**排序，于是 mavlink.ino 在 rc.ino
//   之前编译。宏定义写在 rc.ino 里，mavlink.ino 就完全看不到（"未定义标识符"）。
//   而 board.h 经 i18n.h 被全部 .ino 包含 ⇒ 放这里恰好全局可见，且只定义一次。
//
// RC_SRC：遥控输入源仲裁（v1.2 起的三路共存：AP + ESP-NOW + SBUS）
#define RC_SRC_AUTO   0   // 自动：SBUS 最近 0.3 s 内有有效帧就用 SBUS，否则 ESP-NOW（出厂默认）
#define RC_SRC_ESPNOW 1   // 强制 ESP-NOW：连 SBUS 都不读，彻底隔离（排查用）
#define RC_SRC_SBUS   2   // 强制 SBUS：无帧也不回落（排查用；没接线时飞不了）

// ---------------------------------------------------------------------------
// 编译期自检：单灯板必须至少有一个有效 LED 脚
// ---------------------------------------------------------------------------
// 【为什么需要这条】如果三个灯全是 -1，固件能正常编译、能飞，但完全没有灯语，
// 现场只表现为"灯不亮"，很难定位是配置问题还是硬件问题。这里让它在编译期就炸。
#if (PIN_LED_R < 0) && (PIN_LED_G < 0) && (PIN_LED_B < 0)
	#error "本板型没有任何有效 LED 引脚（PIN_LED_R/G/B 全为 -1）"
#endif
