// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix
// 耀坤整理版修改：https://github.com/YaoKunS/flix

// 电机 PWM 输出
//
// 【耀坤 S3 硬件（2026-08 实测，与主工程 Flix-YaoKun 完全一致）】
//   MOT_RL（左后）GPIO15   MOT_RR（右后）GPIO5
//   MOT_FR（右前）GPIO6    MOT_FL（左前）GPIO16
//   PWM 频率：本文件默认 20 kHz，但两块产品板最终都跑 38 kHz —— 由 config.h 的
//   setDefaults() 按板型覆盖（S3 走上游的 S3/C3 分支，火龙果走 BOARD_HLG_V2 分支）。
//   为什么取 38 kHz（用户 2026-11-06 定案，详见 config.h 处的推导）：
//     · 78 kHz 是 LEDC 在 10-bit 分辨率下的频率天花板（80 MHz ÷ 1024 ≈ 78.125 kHz），
//       再无法上调；但 38k 已完全脱离人耳可闻范围，纹波收益在 40k 以上急剧递减，
//       而开关损耗与 EMI 却是线性上涨 —— 顶格不划算。
//     · 20 kHz 贴着人耳可闻上限（儿童听觉约 17 kHz，余量太小），故不用。
//     · 20~40 kHz 是 1S 有刷 + MOS 直驱的甜点区，38k 居其中。
//
// 【引脚现在由 board.h 提供】上面列的是耀坤 S3 板的实际值，作为记录保留；
//   真正生效的是 board.h 里 PIN_MOTOR_* 宏。换板型只改 board.h，本文件不动。
//
// 【注意 pwmMax = -1 的语义】上游用"占空比模式"（duty 0~100%），不是 PWM 脉宽
//   模式。耀坤电调按占空比调速，保持 -1 即可；若换成需要 1000~2000us 的标准
//   电调，才需要设 MOT_PWM_MIN/MAX。

#include "util.h"
#include "i18n.h"
#include "board.h"   // 板型引脚表：PIN_MOTOR_*

float motors[4]; // 归一化推力，范围 [0..1]

// 【耀坤设置】引脚取自 board.h（S3 板 = 15/5/6/16）；顺序与下面的 MOT_* 常量一一对应
int motorPins[4] = {PIN_MOTOR_RL, PIN_MOTOR_RR, PIN_MOTOR_FR, PIN_MOTOR_FL};
int pwmFrequency = 20000;          // 文件默认（仅"上游通用 ESP32"板型实际采用）；产品板由 config.h 覆盖为 38000
int pwmResolution = 10;
int pwmStop = 0;
int pwmMin = 0;
int pwmMax = -1; // -1 表示占空比模式

const int MOT_RL = 0, MOT_RR = 1, MOT_FR = 2, MOT_FL = 3;

void setupMotors() {
	print(TXT_SETUP_MOTORS);
	// 配置引脚
	for (int i = 0; i < 4; i++) {
		if (motorPins[i] < 0) continue; // 跳过未分配的电机
		ledcAttach(motorPins[i], pwmFrequency, pwmResolution);
		pwmFrequency = ledcChangeFrequency(motorPins[i], pwmFrequency, pwmResolution); // 重新配置时取实际生效值
	}
	sendMotors();
}

void sendMotors() {
	for (int i = 0; i < 4; i++) {
		if (motorPins[i] < 0) continue; // 跳过未分配的电机
		ledcWrite(motorPins[i], getDutyCycle(motors[i]));
	}
}

int getDutyCycle(float value) {
	value = constrain(value, 0, 1);

	if (pwmMax >= 0) { // 脉宽模式
		float pwm = mapf(value, 0, 1, pwmMin, pwmMax);
		if (value == 0) pwm = pwmStop;
		float duty = mapf(pwm, 0, 1000000 / pwmFrequency, 0, (1 << pwmResolution) - 1);
		return round(duty);
	} else { // 占空比模式（耀坤电调用这个）
		return round(value * ((1 << pwmResolution) - 1));
	}
}

bool motorsActive() {
	return motors[0] != 0 || motors[1] != 0 || motors[2] != 0 || motors[3] != 0;
}

void testMotor(int n, float thrust) {
	print(TXT_MOT_TEST, n);
	motors[n] = thrust;
	delay(50); // ESP32 需等当前 PWM 周期结束才改占空比（见 arduino-esp32 issue #5306）
	sendMotors();
	pause(3);
	motors[n] = 0;
	sendMotors();
	print(TXT_MOT_DONE);
}
