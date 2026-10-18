// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix
// 耀坤整理版修改：https://github.com/YaoKunS/flix

// 主固件文件（setup / loop）
//
// 【主循环顺序的讲究（第 4.1 节的"为什么是这个顺序"在这里落地）】
//   readIMU()      —— 必须最先：所有控制都基于本帧的 IMU 数据
//   step()         —— 紧接着更新时间：dt 是从上一次 step 到现在的差，
//                     放在 IMU 之后才能让 dt 与"这帧数据的新鲜度"对应
//   readRC()       —— 拿到本帧飞手输入（SBUS；ESP-NOW 的输入在 processMavlink 里）
//   estimate()     —— 姿态解算（用 gyro/acc + dt）
//   control()      —— 三层环 + 失效保护（用姿态与输入）
//   sendMotors()   —— 输出 PWM，越早越好，避免控制延迟
//   handleConsole/processMavlink —— 通信放在控制之后：通信再忙也不能推迟电机输出
//   readVoltage/logData/syncParameters —— 低频事务放最后
//   updateLED()    —— 灯语/蜂鸣器是纯非阻塞状态机，放最后不影响时序
//
// 这段顺序是从上游继承的，**不要为了"看起来整齐"去调换**：把通信挪到
// sendMotors() 之前，会让电机输出抖动几毫秒，PID 表现为高频抖动。

#include "vector.h"
#include "quaternion.h"
#include "util.h"
#include "i18n.h"

extern float t, dt;
extern float controlRoll, controlPitch, controlYaw, controlThrottle, controlMode;
extern Vector gyro, acc;
extern Vector rates;
extern Quaternion attitude;
extern bool landed;
extern bool imuOK;
extern float motors[4];

// ============================================================================
// 【固件指纹】开机横幅里的身份串
// ----------------------------------------------------------------------------
// 用途：现场快速确认"这块板子烧的是哪一版固件、属于哪条产品线"。
// 与耀坤自研飞控 YaoKunDrone、开源 ESP-Drone 整理版保持同一套指纹语义：
//    组织 | 产品名 | 固件家族 | 品牌
// 排查时只需看串口第一屏，无需查 SHA-256。
// 参数表里也暴露同名项的读取入口（只读语义，见 parameters.ino 注释）。
// ============================================================================
const char *identityString = "YaoKunS | 耀坤S3无人机 | Flix | 耀坤智联";

void setup() {
	setupConsole();
	print(TXT_BOOT_START);
	print("%s\n", identityString); // 指纹串：紧跟在开机首行之后
	print(TXT_BOOT_VERSION);

	setupParameters();
	setupPower();

	// ---- 灯 / 蜂鸣器 ----
	// 蓝灯常亮 = 初始化中；短鸣一声 = 开始
	print(TXT_SETUP_LED);
	setupLED();
	setBlue(true);
	buzzerPlay('S');
	runBuzzerFor(250); // 只在 setup 期阻塞播放，此时未解锁，安全

	setupMotors();
	setupWiFi();
	setupIMU();
	setupRC();

	// ---- 初始化结束提示 ----
	setBlue(false);
	if (!imuOK) {
		// IMU 不可用：用红灯常亮 + 长鸣明确告警，避免用户以为"正常但就是不解锁"
		setRed(true);
		buzzerPlay('L');
		runBuzzerFor(700);
	} else {
		buzzerPlay('D'); // 双短鸣 = 就绪
		runBuzzerFor(600);
	}

	print(TXT_BOOT_DONE);
}

void loop() {
	readIMU();
	step();
	readRC();
	estimate();
	control();
	sendMotors();
	handleConsole();
	processMavlink();
	learnPeerIfNeeded(); // 自动学习遥控器 MAC 并登记到 ESP-NOW peer 表
	readVoltage();
	logData();
	syncParameters();
	updateLED(); // 刷新灯语与蜂鸣器状态（非阻塞）
}
