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

// ============================================================================
// 控制台发送环形缓冲长度（字节）
// ----------------------------------------------------------------------------
// 用途：setupConsole() 里用它把 HWCDC 的 TX 环形缓冲从默认 256B 扩到 4KB。
// 为什么需要：见下面 setupConsole() 里的【🔴 help 菜单只吐前半截】注释。
// ============================================================================
#define CONSOLE_TX_QUEUE_LEN 4096

void setupConsole() {
	Serial.begin(115200);
#if SOC_USB_SERIAL_JTAG_SUPPORTED
	Serial.setTxTimeoutMs(0); // never block on usb write

	// ------------------------------------------------------------------------
	// 【🔴 2026-10-18 修复：输入 help 只看到开机横幅，看不到命令菜单】
	//
	// 现象：烧录后敲 `help`，串口监视器只显示上面那段品牌条（约 700 字节），
	//       "可用命令：" 标题和后面 29 行命令一条都没有；再敲一次 `help`
	//       还是只有横幅。用 `time`/`rc` 这类短输出的命令倒是一切正常。
	//
	// 根因（三步凑一起才炸）：
	//   ① motd 是**一整条** C 字符串，长度 1718 字节（品牌条 + 30 行命令表）；
	//      print() 里 vsnprintf 之后是一次 Serial.print(buf) 整体写出。
	//   ② ESP32-S3 原生 USB 走 HWCDC，它的发送环形缓冲**默认只有 256 字节**
	//      （core 的 HWCDC.cpp 里 setTxBufferSize(256)）。
	//   ③ 上面那句 setTxTimeoutMs(0) 让写操作"绝不阻塞"——HWCDC::write() 里
	//      xRingbufferSend 以 0 tick 超时，缓冲装不下的部分**当场 break 静默丢弃**。
	//   ⇒ 1718 字节一次性塞进 256 字节的缓冲，只有前 ~256 字节能进，
	//     USB 中断还没来得及往主机搬，其余 1400+ 字节全部丢掉。
	//     而丢弃点正好落在品牌条末尾，所以看起来像"菜单丢了"。
	//
	// 修法：把 TX 缓冲扩到 4KB，整份 motd 一次塞进环形缓冲，由 USB 中断异步
	//      搬给主机。主循环依旧零阻塞（setTxTimeoutMs(0) 保持不变），
	//      飞行实时性不受影响。代价：4KB 堆内存。
	//
	// 注意：只扩大缓冲还不够——"最后一段"输出会滞留在缓冲里等下一次写入才被
	//      带走（HWCDC 中断只在 write() 结尾重新使能）。所以 handleConsole()
	//      收尾还要调 consoleFlushTail() 补一刀，两处配合才完整。
	// ------------------------------------------------------------------------
	Serial.setTxBufferSize(CONSOLE_TX_QUEUE_LEN);
#endif
}

// ============================================================================
// 控制台"尾部输出"推送
// ----------------------------------------------------------------------------
// 【现象】命令的**最后一段输出**在串口监视器里看不见；随便再敲一条命令，
//   那段"消失"的输出就跟着一起冒出来了。
// 【根因】HWCDC 的发送链路是：write() → 塞进发送环形缓冲 → 由
//   USB_SERIAL_JTAG_INTR_SERIAL_IN_EMPTY 中断把数据搬进硬件 FIFO。
//   core 的中断服务里顺序是：①先关中断 → ②从环形缓冲取数据 → ③只有取到了
//   才重新开中断。如果那一刻环形缓冲恰好是空的（前一批刚被搬完），③不执行，
//   **中断保持关闭**，直到下一次 write() 结尾才重新打开。
//   ⇒ "最后一次写入"的数据滞留在缓冲里没人搬，要靠**下一次写入**带走。
// 【修法】命令收尾"补写一个字符 + 等环形缓冲排空"，最多等 timeoutMs 毫秒。
//   补写会重新使能 USB 中断、把滞留数据带走；缓冲一空就立刻返回。
// 【参数 nudge：补写哪个字符】
//   必须传 ' '（空格）：不可见，且天然充当分隔符，**不会把一行拆成多行**。
//   踩坑记录：原先每推一次都补写 '\n'，结果一行数字被切断成好几行。
// 【为什么不能用 Serial.flush()】本项目设了 setTxTimeoutMs(0)，而 HWCDC::flush()
//   在 tx_timeout_ms==0 时 tries 一上来就是 0 → 直接走 flushTXBuffer(NULL, 0)，
//   会把整个发送缓冲**丢弃**并把自己标记成"未连接"。严禁使用。
// 【排空判定：不要写 `>= CONSOLE_TX_QUEUE_LEN`】TX 环形缓冲是 BYTEBUF 类型，
//   可用空间最大值是"容量 − 1"（留 1 字节区分满/空），那个条件永远不成立。
//   改为"连续两拍可用空间不再增长"即判定排空，与缓冲容量解耦。
// 【安全性】只在"命令执行完"时调用，且有超时上限（150ms），不长时间阻塞主循环。
// 参数：timeoutMs 最长等待毫秒数；nudge 补写的字符（固定传 ' '）。无返回值。
// ============================================================================
void consoleFlushTail(uint32_t timeoutMs, char nudge) {
	Serial.write(nudge); // 补写一次：重新使能 USB 中断，把滞留数据带走
	uint32_t start = millis();
	int lastFree = Serial.availableForWrite();
	int stuck = 0;
	while (millis() - start < timeoutMs) {
		delay(5);
		int freeNow = Serial.availableForWrite();
		if (freeNow == lastFree) {
			if (++stuck >= 2) return; // 连续两拍没进展 ⇒ 已排空（或主机没在读），收工
		} else {
			stuck = 0; // 还在往外搬，继续等
			lastFree = freeNow;
		}
	}
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
		// 开机横幅也补一刀：它是全工程最长的一条输出（1718 字节），
		// 尾部同样会滞留在发送缓冲里，不推一把就只能看到前半截。
		consoleFlushTail(200, ' ');
		showMotd = false;
	}

	while (Serial.available()) {
		char c = Serial.read();
		if (c == '\n' || c == '\r') {
			// 【🔴 重入 bug 修复】必须先"取出副本 + 清空"，再执行 doCommand。
			//   现象：串口敲一次 `cr`，标定向导却**跑了两轮**（打印两张汇总表）。
			//   根因：doCommand 里的**阻塞命令**（cr/ca/mfr… 会调用 pause()）会在
			//     阻塞期间经 pause() → handleConsole() **重入本函数**；
			//     而原写法是 `doCommand(input); input.clear();`——清空发生在 doCommand
			//     **返回之后**，于是阻塞期间 input 仍等于 "cr"。
			//     此时串口监视器若把回车设成 "\r\n"，尾随的 '\n' 会被重入的
			//     handleConsole 读到 → 拿**残留的 "cr"** 又执行一遍。
			//   修法：先把 input 拷进局部变量并立刻 clear()，再做阻塞调用。
			//     这样重入时 input 已是空串，`command.isEmpty()` 直接挡掉。
			String cmd = input;
			input.clear();
			if (!cmd.isEmpty()) {
				doCommand(cmd);
				// 命令跑完，把"最后一段输出"顶给主机（见 consoleFlushTail 注释）。
				// 补写空格（不是换行）：不可见、不拆行、也不多出空行。
				consoleFlushTail(150, ' ');
			}
		} else {
			input += c;
		}
	}
}
