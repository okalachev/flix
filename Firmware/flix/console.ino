// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix
// 耀坤整理版修改：https://github.com/YaoKunS/flix

// 命令行控制台（串口 / USB-CDC 上的交互界面）
//
// 【耀坤整理版的改动】
//   1) 全部界面文字改为中文（通过 i18n.h 的 TXT_* 宏；定义 FLIX_LANG_EN 可编英文版）。
//   2) 开机横幅换成耀坤品牌 + 版权声明：GPL 要求保留原作者署名，所以
//      "(C) Oleg Kalachev" 与上游仓库地址必须保留，耀坤信息追加在其后。
//   3) 新增 `ch` 命令：查看/设置 ESP-NOW 信道 —— 遥控器与飞机必须同信道，
//      没有这条命令就得改代码重编，联调时极其痛苦。

#include "pid.h"
#include "vector.h"
#include "util.h"
#include "filter.h"
#include "i18n.h"

extern const int MOT_RL, MOT_RR, MOT_FR, MOT_FL;
extern const int RAW, ACRO, STAB, AUTO;
extern const int W_AP, W_STA, W_ESPNOW;
extern float t, dt, loopRate;
extern uint16_t channels[16];
extern float controlTime;
extern int mode;
extern bool armed;
extern LowPassFilter<Vector> gyroBiasFilter;
extern float voltage;
extern uint32_t tRewinds;
extern int espnowChannel;

// ---------------------------------------------------------------------------
// 开机横幅（motd）
//
// 设计取舍：
//   · 不再用上游那个 6 行 ASCII 大字 "FLIX" —— 它占 6 行屏幕，而第一屏最该看到的
//     是"这是什么、谁做的、能敲什么"。改成品牌条 + 命令表。
//   · 版权行不能删：flix 是 GPL 项目，分发二进制必须保留原作者署名与源码地址。
//     所以这里先写 Oleg，再写耀坤；这不是客套，是许可证义务。
// ---------------------------------------------------------------------------
const char* motd =
"\n"
"==============================================================\n"
"  耀坤S3 飞控（Flix 整理版）   YaoKunS Flight Controller\n"
"  硬件：立创 ESP32-S3 + MPU6500/MPU6050   遥控：ESP-NOW\n"
"--------------------------------------------------------------\n"
"  开源：https://github.com/YaoKunS/flix\n"
"  淘宝：耀坤智联  https://shop35432590.taobao.com/\n"
"  B 站：耀坤智联  https://space.bilibili.com/3707011699050767/\n"
"--------------------------------------------------------------\n"
"  内核 (C) Oleg Kalachev   https://github.com/okalachev/flix\n"
"  本固件由耀坤智联二次开发（目录整理 / 中文界面 / 硬件适配）\n"
"==============================================================\n"
"\n"
TXT_HELP_TITLE
TXT_HELP_HELP
TXT_HELP_P
TXT_HELP_P_STR
TXT_HELP_P_SET
TXT_HELP_PRESET
TXT_HELP_TIME
TXT_HELP_IMU
TXT_HELP_CA
TXT_HELP_ST
TXT_HELP_ARM
TXT_HELP_MODE
TXT_HELP_RC
TXT_HELP_CR
TXT_HELP_PW
TXT_HELP_WIFI
TXT_HELP_WIFI_SET
TXT_HELP_AP
TXT_HELP_STA
TXT_HELP_ESPNOW
TXT_HELP_CH
TXT_HELP_MOT
TXT_HELP_LOG
TXT_HELP_MFR
TXT_HELP_SYS
TXT_HELP_RESET
TXT_HELP_REBOOT
"\n";

// Always redirect Serial to USB on ESP32-S3/ESP32-C3
#if SOC_USB_SERIAL_JTAG_SUPPORTED && ARDUINO_USB_MODE
#if !ARDUINO_USB_CDC_ON_BOOT
HWCDC HWCDCSerial;
#endif
#undef Serial
#define Serial HWCDCSerial
#endif

void setupConsole() {
	Serial.begin(115200);
#if SOC_USB_SERIAL_JTAG_SUPPORTED
	Serial.setTxTimeoutMs(0); // never block on usb write
#endif
}

void print(const char* format, ...) {
	char buf[3000];
	va_list args;
	va_start(args, format);
	vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);
	Serial.print(buf);
	mavlinkPrint(buf);
}

void pause(float duration) {
	float start = t;
	while (t - start < duration) {
		step();
		handleConsole();
		processMavlink();
		delay(50);
	}
}

void doCommand(String str, bool echo = false) {
	// parse command
	String command, arg0, arg1;
	splitString(str, command, arg0, arg1);
	if (command.isEmpty()) return;

	// echo command
	if (echo) {
		print("> %s\n", str.c_str());
	}

	command.toLowerCase();

	// 执行命令
	if (command == "help" || command == "motd") {
		print("%s\n", motd);
	} else if (command == "p" && arg1 == "") {
		printParameters(arg0.c_str());
	} else if (command == "p") {
		bool success = setParameter(arg0.c_str(), arg1.toFloat());
		if (success) {
			print("%s = %g\n", arg0.c_str(), getParameter(arg0.c_str()));
		} else {
			print(TXT_PARAM_NOT_FOUND, arg0.c_str());
			print(TXT_HINT_HELP);
		}
	} else if (command == "preset") {
		resetParameters();
	} else if (command == "time") {
		print(TXT_TIME, t);
		print(TXT_LOOP_RATE, loopRate);
		print(TXT_DT, dt);
		print(TXT_TIMEBASE, tRewinds);
	} else if (command == "imu") {
		printIMUInfo();
		printIMUCalibration();
		print(TXT_IMU_LANDED, landed);
	} else if (command == "st") {
		print(TXT_ST_RATES, rates.x, rates.y, rates.z);
		print(TXT_ST_ATT, attitude.w, attitude.x, attitude.y, attitude.z);
		print(TXT_ST_EULER, degrees(attitude.getRoll()), degrees(attitude.getPitch()), degrees(attitude.getYaw()));
		print(TXT_ST_LANDED, landed);
	} else if (command == "arm") {
		armed = true;
	} else if (command == "disarm") {
		armed = false;
	} else if (command == "raw") {
		mode = RAW;
	} else if (command == "stab") {
		mode = STAB;
	} else if (command == "acro") {
		mode = ACRO;
	} else if (command == "auto") {
		mode = AUTO;
	} else if (command == "rc") {
		print(TXT_RC_CHANNELS);
		for (int i = 0; i < 16; i++) {
			print("%u ", channels[i]);
		}
		print(TXT_RC_CONTROLS, controlRoll, controlPitch, controlYaw, controlThrottle, controlMode);
		print(TXT_RC_TIME, controlTime);
		print(TXT_RC_MODE, getModeName());
		print(TXT_RC_ARMED, armed);
	} else if (command == "pw") {
		print(TXT_VOLTAGE, voltage);
	} else if (command == "wifi" && arg0 == "") {
		printWiFiInfo();
	} else if (command == "wifi") {
		setWiFiMode(arg0);
	} else if (command == "ap") {
		configWiFi(W_AP, arg0.c_str(), arg1.c_str());
	} else if (command == "sta") {
		configWiFi(W_STA, arg0.c_str(), arg1.c_str());
	} else if (command == "espnow") {
		configWiFi(W_ESPNOW, arg0.c_str(), arg1.c_str());
	} else if (command == "ch") {
		// 【为什么加这条命令】ESP-NOW 是"同信道才能通"的：遥控器端代码里钉死了
		// 一个信道（耀坤英文版遥控器 = 6，见其 Control.hpp 的 esp_wifi_set_channel(6)）。
		// 飞机端若不同步，表现就是"遥控器显示已连接但飞机毫无反应"，且没有任何报错，
		// 极难排查。以前只能改源码重编，现在一条命令就能对齐。
		// 不带参数 = 查看；带参数 = 设置（走参数层，自动存 NVS，重启生效）。
		if (arg0 == "") {
			print(TXT_WIFI_CHANNEL, espnowChannel);
		} else {
			int ch = arg0.toInt();
			if (ch >= 1 && ch <= 13) {
				setParameter("ESPNOW_CHANNEL", ch);
				print(TXT_CH_SET, ch);
			} else {
				print(TXT_CH_RANGE);
			}
		}
	} else if (command == "mot") {
		print(TXT_MOT_OUT, motors[MOT_FR], motors[MOT_FL], motors[MOT_RR], motors[MOT_RL]);
	} else if (command == "log") {
		printLogHeader();
		if (arg0 == "dump") printLogData();
	} else if (command == "cr") {
		calibrateRC();
	} else if (command == "ca") {
		calibrateAccel();
	} else if (command == "mfr") {
		testMotor(MOT_FR, arg0.isEmpty() ? 0.2 : arg0.toFloat());
	} else if (command == "mfl") {
		testMotor(MOT_FL, arg0.isEmpty() ? 0.2 : arg0.toFloat());
	} else if (command == "mrr") {
		testMotor(MOT_RR, arg0.isEmpty() ? 0.2 : arg0.toFloat());
	} else if (command == "mrl") {
		testMotor(MOT_RL, arg0.isEmpty() ? 0.2 : arg0.toFloat());
	} else if (command == "sys") {
#ifdef ESP32
		print(TXT_SYS_CHIP, ESP.getChipModel());
		print(TXT_SYS_TEMP, temperatureRead());
		print(TXT_SYS_RAM, ESP.getHeapSize() / 1024);
		print(TXT_SYS_HEAP, ESP.getFreeHeap() / 1024);
		print(TXT_SYS_FIRMWARE);
		// Print tasks table
		print(TXT_SYS_TASKS);
		int taskCount = uxTaskGetNumberOfTasks();
		TaskStatus_t *systemState = new TaskStatus_t[taskCount];
		uint32_t totalRunTime;
		uxTaskGetSystemState(systemState, taskCount, &totalRunTime);
		for (int i = 0; i < taskCount; i++) {
			String core = systemState[i].xCoreID == tskNO_AFFINITY ? "*" : String(systemState[i].xCoreID);
			int cpuPercentage = systemState[i].ulRunTimeCounter / (totalRunTime / 100);
			print("%-5d%-20s%-7d%-6d%-6s%d\n", systemState[i].xTaskNumber, systemState[i].pcTaskName,
				systemState[i].usStackHighWaterMark, systemState[i].uxCurrentPriority, core.c_str(), cpuPercentage);
		}
		delete[] systemState;
#endif
	} else if (command == "reset") {
		attitude = Quaternion();
		gyroBiasFilter.reset();
	} else if (command == "reboot") {
		ESP.restart();
	} else {
		print(TXT_INVALID_CMD, command.c_str());
		print(TXT_HINT_HELP);
	}
}

void handleConsole() {
	static bool showMotd = true;
	static String input;

	if (showMotd) {
		print("%s\n", motd);
		showMotd = false;
	}

	while (Serial.available()) {
		char c = Serial.read();
		if (c == '\n' || c == '\r') {
			doCommand(input);
			input.clear();
		} else {
			input += c;
		}
	}
}
