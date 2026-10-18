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

SBUS rc(Serial1);

// 【耀坤设置】-1 = 不启用 SBUS（默认，走 ESP-NOW）；GPIO1 = 板上 EXT01 的 SBUS 输入
int rcRxPin = -1;

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
}

bool readRC() {
	if (rcRxPin < 0) return false;
	if (!rc.read()) return false;

	rc.getChannels(channels);
	normalizeRC();
	controlTime = t;
	return true;
}

// 遥控硬件是否在用（SBUS 接了就认为在用；未接则由 ESP-NOW 侧负责刷新 controlTime）
bool rcHardwarePresent() {
	return rcRxPin >= 0;
}

void normalizeRC() {
	float controls[16];
	for (int i = 0; i < 16; i++) {
		controls[i] = mapf(channels[i], channelZero[i], channelMax[i], 0, 1);
	}
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
