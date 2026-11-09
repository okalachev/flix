// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix
// 耀坤整理版修改：https://github.com/YaoKunS/flix

// 遥控接收（SBUS 接收机 + ESP-NOW 摇杆值归一化）
//
// 【耀坤整理版的改动】
//   1) 出厂默认**不启用 SBUS**（rcRxPin = -1）—— 耀坤主用遥控是 ESP-NOW，
//      SBUS 只是备选。要用 SBUS 就 `p RC_RX_PIN 1` 然后重启。
//   2) 新增 rcHardwarePresent()：判"遥控到底有没有接"。上游 readRC() 在 SBUS
//      未接时直接 return false，但 controlTime 仍会被 MAVLink 分支刷新，
//      导致安全层以为"遥控一直在动"，失联保护形同虚设。
//   3) 校准步骤提示改中文 + 打印完成项数。
//   4) 【与上游的差异】normalizeRC() 里 throttle 用 channelZero 作零位，
//      而 ESP-NOW 路直接用 MAVLink 的 -1000/+1000 归一化，两条路径互不干扰：
//      SBUS 只用 SBUS，ESP-NOW 只用 mavlink.ino 的 MANUAL_CONTROL 分支。

#include <SBUS.h>
#include "util.h"
#include "i18n.h"
#include "board.h"   // 板型引脚表：PIN_RC_RX

SBUS rc(Serial1);

// 【引脚来自 board.h】-1 = 不启用 SBUS（走 ESP-NOW）。
// v1.2 起耀坤 S3 板出厂为 1（板上 EXT01 的 SBUS 输入），火龙果 2.0 为 4。
int rcRxPin = PIN_RC_RX;

// ===========================================================================
// 【v1.2 新增】三路遥控共存（AP + ESP-NOW + SBUS）的输入源仲裁
// ---------------------------------------------------------------------------
// 背景：v1.1 及以前的 SBUS 与 ESP-NOW 是「各写各的同一个全局变量」——
//   readRC() 写 controlRoll/Pitch/Yaw/Throttle，processMavlink() 的
//   MANUAL_CONTROL 分支也写同一组，两者都没有仲裁。主循环顺序是
//   readRC() → control() → processMavlink()，所以 ESP-NOW 每轮都后写，
//   SBUS 14 ms 一帧、ESP-NOW 约 20 ms 一帧，两个值交替覆盖 = 杆量跳变。
//   这种状态只能叫"打架"，不能叫"共存"。
//
// 现在的规则（用户 2026-11-09 拍板：SBUS 优先）：
//   RC_SRC = 0（出厂默认，自动）：SBUS 最近 RC_SBUS_HOLD 秒内有有效帧 → SBUS 说了算；
//                                 否则交回 ESP-NOW。插上自动接管、拔掉自动回落。
//   RC_SRC = 1（强制 ESP-NOW）：连读都不读 SBUS（readRC 直接 return），彻底隔离。
//   RC_SRC = 2（强制 SBUS）    ：SBUS 无帧也不回落 —— 专门用来排查"到底是不是
//                                 SBUS 这一路的问题"，代价是没接 SBUS 时飞不了。
//
// 为什么"优先 SBUS"而不是"优先 ESP-NOW"：ESP-NOW 是常开的，让它优先会让 SBUS
//   永远抢不到；而"插上线"本身就是操作者的明确意图，优先它才符合直觉。
//
// RC_SRC_AUTO / RC_SRC_ESPNOW / RC_SRC_SBUS 三个宏定义在 board.h 的"通用行为常量"里
// （不能写在本文件：合并顺序上 mavlink.ino 在 rc.ino 之前，看不到这里的宏）。
// ===========================================================================

int rcSrc = RC_SRC_AUTO;      // 参数 RC_SRC 的实体，可在串口用 `p RC_SRC 1` 改，自动存盘

// SBUS 最后一次"收到有效帧"的时刻。初值 NAN = 从未收到过。
// 用 NAN 而不是 0：t 是从开机起算的单调时间，初值 0 会让"开机第一秒"被误判成
// "刚刚收到过帧"，从而错误地把控制权判给 SBUS（而那时 SBUS 根本没接线）。
float sbusLastTime = NAN;

// SBUS 掉线判定阈值（秒）。SBUS 标准帧率 14 ms（约 71 Hz），
// 取 0.3 s ≈ 21 帧：既不会因为丢一两帧就抢走控制权，
// 又能在拔线的瞬间（<0.3 s）把控制权交回 ESP-NOW，不会出现"拔了线还在等"的僵局。
const float RC_SBUS_HOLD = 0.3f;

// 判定"某个通道是否已标定"的最小行程。SBUS 全量程约 1639（172~1811），
// 取 100 = 量程的 6%，远小于任何正常摇杆行程，同时能挡掉
// "channelZero == channelMax == 0"（全新板、NVS 空）这个除零炸弹。
const int RC_MIN_SPAN = 100;

// 每个通道"未标定"警告只打一次，避免 normalizeRC() 每帧刷屏把控制台冲掉
bool rcUncalWarned[16] = {};

uint16_t channels[16]; // 原始通道值
int channelZero[16];   // 通道零位（校准值）
int channelMax[16];    // 通道满量程（校准值）

float controlRoll, controlPitch, controlYaw, controlThrottle; // 飞手输入，范围 [-1, 1]
float controlMode = NAN;
float controlTime = NAN; // 最后一次控制输入更新的时刻（安全层据此判失联）

int rollChannel = -1, pitchChannel = -1, throttleChannel = -1, yawChannel = -1, modeChannel = -1; // 通道映射

void setupRC() {
	if (rcRxPin < 0) {
		print(TXT_RC_SBUS_OFF, rcRxPin);
		return;
	}
	print(TXT_SETUP_RC);
	rc.begin(rcRxPin);
	// 开机把"SBUS 已启用"讲明白：**不接接收机不报错、也不影响 ESP-NOW**。
	// 不写这句的话，用户看到 SBUS 被打开会以为"必须接线才能飞"。
	print(TXT_RC_SBUS_ON, rcRxPin);
}

// SBUS 这一路现在是否"在线"：最近 RC_SBUS_HOLD 秒内解析出过有效帧。
// 注意必须是 valid(sbusLastTime) —— 从未收到过帧时它是 NAN，
// NAN 参与比较恒为 false，这正是我们想要的"没接线 = 不在线"。
bool sbusActive() {
	return rcRxPin >= 0 && valid(sbusLastTime) && (t - sbusLastTime < RC_SBUS_HOLD);
}

// 给 `rc` 命令用：返回当前真正生效的输入源名称（走 i18n，中英自动切）。
// 这里刻意区分"强制模式"与"在线状态"：强制 SBUS 但没接线时仍报 SBUS，
// 因为那是用户自己锁定的源，报别的反而误导排查。
const char *rcSourceLabel() {
	const bool espnowAlive = valid(controlTime) && (t - controlTime < 1.0f);
	if (rcSrc == RC_SRC_SBUS) return TXT_RC_SRC_SBUS;
	if (rcSrc == RC_SRC_ESPNOW) return espnowAlive ? TXT_RC_SRC_ESPNOW : TXT_RC_SRC_NONE;
	if (sbusActive()) return TXT_RC_SRC_SBUS;
	return espnowAlive ? TXT_RC_SRC_ESPNOW : TXT_RC_SRC_NONE;
}

bool readRC() {
	if (rcRxPin < 0) return false;
	// 【v1.2】强制 ESP-NOW 模式下连读都不读：不读就不会更新 channels[]、
	// 不会刷新 sbusLastTime，从根上排除 SBUS 抢值 —— 这是"锁源排查"该有的语义。
	if (rcSrc == RC_SRC_ESPNOW) return false;
	if (!rc.read()) return false;

	rc.getChannels(channels);
	normalizeRC();
	sbusLastTime = t; // 仲裁依据：只有真正解析出有效帧才更新时间戳
	controlTime = t;
	return true;
}

// 遥控硬件是否在用（SBUS 接了就认为在用；未接则由 ESP-NOW 侧负责刷新 controlTime）
bool rcHardwarePresent() {
	return rcRxPin >= 0;
}

// 对 5 个"已映射"的通道检查标定是否可用，缺标定的每个通道只提示一次。
// 为什么只提示已映射的：其余 11 个通道用户根本没用到，提示它们纯属噪音。
void warnUncalibratedChannels() {
	const int mapped[5] = {rollChannel, pitchChannel, throttleChannel, yawChannel, modeChannel};
	for (int k = 0; k < 5; k++) {
		const int ch = mapped[k];
		if (ch < 0 || ch > 15) continue;
		if (abs(channelMax[ch] - channelZero[ch]) >= RC_MIN_SPAN) continue;
		if (rcUncalWarned[ch]) continue;
		rcUncalWarned[ch] = true;
		print(TXT_RC_UNCAL, ch, channelZero[ch], channelMax[ch], ch, ch);
	}
}

void normalizeRC() {
	float controls[16];
	for (int i = 0; i < 16; i++) {
		// 【v1.2 防满油·安全级】未标定通道保护。
		//   现象：出厂（NVS 空）时 channelZero / channelMax 全是 0，
		//        mapf(x, 0, 0, 0, 1) = (x-0)*1/(0-0) = ±inf。
		//   后果：control.ino:55 是 `thrustTarget = controlThrottle;`，没有任何 clamp，
		//        inf 会一路进混控 ⇒ 一推杆就满油或 NaN，直接炸机。
		//   修法：行程不足 RC_MIN_SPAN 的通道一律输出 0（宁可不动），并警告一次。
		if (abs(channelMax[i] - channelZero[i]) < RC_MIN_SPAN) {
			controls[i] = 0;
			continue;
		}
		controls[i] = mapf(channels[i], channelZero[i], channelMax[i], 0, 1);
	}
	warnUncalibratedChannels();
	// 更新控制量
	controlRoll = rollChannel < 0 ? 0 : controls[rollChannel];
	controlPitch = pitchChannel < 0 ? 0 : controls[pitchChannel];
	controlYaw = yawChannel < 0 ? 0 : controls[yawChannel];
	controlThrottle = throttleChannel < 0 ? 0 : controls[throttleChannel];
	controlMode = modeChannel < 0 ? NAN : controls[modeChannel]; // 未映射模式开关时该值无效
}

void calibrateRC() {
	if (rcRxPin < 0) {
		print(TXT_RC_DISABLED, rcRxPin);
		print(TXT_RC_ESPNOW_NOCAL);
		return;
	}

	uint16_t zero[16];   // 零位采样
	uint16_t center[16]; // 中位采样
	uint16_t _[16];      // 未使用的数据
	print(TXT_CR_1);
	pause(3);
	calibrateRCChannel(NULL, _, zero, TXT_CR_2);
	calibrateRCChannel(&throttleChannel, zero, _, TXT_CR_3);
	calibrateRCChannel(NULL, _, center, TXT_CR_4);
	calibrateRCChannel(&yawChannel, center, _, TXT_CR_5);
	calibrateRCChannel(&pitchChannel, zero, _, TXT_CR_6);
	calibrateRCChannel(&rollChannel, zero, _, TXT_CR_7);
	calibrateRCChannel(&modeChannel, zero, _, TXT_CR_8);
	printRCCalibration();

	// 统计成功的项数，明确告诉用户"到底校准上了几路"
	// （上游只打印表格，用户看不出哪几路其实没识别到）
	int ok = 0;
	if (rollChannel >= 0) ok++;
	if (pitchChannel >= 0) ok++;
	if (yawChannel >= 0) ok++;
	if (throttleChannel >= 0) ok++;
	if (modeChannel >= 0) ok++;
	print(TXT_CR_SUMMARY, ok, 5);
}

void calibrateRCChannel(int *channel, uint16_t in[16], uint16_t out[16], const char *str) {
	print("%s", str);
	pause(3);
	for (int i = 0; i < 30; i++) readRC(); // 最多尝试刷新 30 次
	memcpy(out, channels, sizeof(channels));

	if (channel == NULL) return; // 无需识别通道

	// 找出 in 与 out 之间变化最大的通道
	int ch = -1, diff = 0;
	for (int i = 0; i < 16; i++) {
		if (abs(out[i] - in[i]) > diff) {
			ch = i;
			diff = abs(out[i] - in[i]);
		}
	}
	if (ch >= 0 && diff > 10) { // 差值阈值为 10
		*channel = ch;
		channelZero[ch] = in[ch];
		channelMax[ch] = out[ch];
	} else {
		*channel = -1;
	}
}

void printRCCalibration() {
	print(TXT_CR_HEADER);
	print("横滚      %-7d%-7d%-7d\n", rollChannel, rollChannel < 0 ? 0 : channelZero[rollChannel], rollChannel < 0 ? 0 : channelMax[rollChannel]);
	print("俯仰      %-7d%-7d%-7d\n", pitchChannel, pitchChannel < 0 ? 0 : channelZero[pitchChannel], pitchChannel < 0 ? 0 : channelMax[pitchChannel]);
	print("偏航      %-7d%-7d%-7d\n", yawChannel, yawChannel < 0 ? 0 : channelZero[yawChannel], yawChannel < 0 ? 0 : channelMax[yawChannel]);
	print("油门      %-7d%-7d%-7d\n", throttleChannel, throttleChannel < 0 ? 0 : channelZero[throttleChannel], throttleChannel < 0 ? 0 : channelMax[throttleChannel]);
	print("模式开关  %-7d%-7d%-7d\n", modeChannel, modeChannel < 0 ? 0 : channelZero[modeChannel], modeChannel < 0 ? 0 : channelMax[modeChannel]);
}
