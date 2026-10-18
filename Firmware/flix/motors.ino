// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix
// 耀坤整理版修改：https://github.com/YaoKunS/flix

// 电机 PWM 输出
//
// 【耀坤 S3 硬件（2026-08 实测，与主工程 Flix-YaoKun 完全一致）】
//   MOT_RL（左后）GPIO15   MOT_RR（右后）GPIO5
//   MOT_FR（右前）GPIO6    MOT_FL（左前）GPIO16
//   PWM 频率 20 kHz：耀坤电调（1S 有刷 MOS 驱动）在此频率下噪声小、线性好。
//   上游默认 78 kHz 是为无刷+专用电调准备的，用在耀坤板子上会"叫"且推力曲线异常。
//
// 【注意 pwmMax = -1 的语义】上游用"占空比模式"（duty 0~100%），不是 PWM 脉宽
//   模式。耀坤电调按占空比调速，保持 -1 即可；若换成需要 1000~2000us 的标准
//   电调，才需要设 MOT_PWM_MIN/MAX。

#include "util.h"
#include "i18n.h"

float motors[4]; // 归一化推力，范围 [0..1]

// 【耀坤设置】引脚按 PCB 实测填写；顺序与下面的 MOT_* 常量一一对应
int motorPins[4] = {15, 5, 6, 16}; // 依次为 MOT_RL / MOT_RR / MOT_FR / MOT_FL
int pwmFrequency = 20000;          // 耀坤电调 20 kHz（上游默认 78000）
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
