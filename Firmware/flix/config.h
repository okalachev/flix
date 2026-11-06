// Copyright (c) 2026 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix
// 耀坤整理版修改：https://github.com/YaoKunS/flix

// Parameter defaults

#pragma once

#include "board.h"   // 板型编号（FLIX_BOARD）与引脚表；本文件要用它区分板型

void setDefaults() {
	// Set defaults here

	// 电机 PWM 频率：S3/C3 沿用上游的 38 kHz（下面这一行原样保留，S3 代码路径零改动）
	#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3)
		pwmFrequency = 38000;
	#endif

	// -----------------------------------------------------------------------
	// 【火龙果 2.0】PWM 频率同样统一为 38 kHz（用户 2026-11-06 定案，两块板实测）
	//   为什么不直接用 motors.ino 里的 20000：20 kHz 正好贴着人耳可闻上限
	//   （成年人约 15~17 kHz、儿童约 20 kHz），在安静环境里仍可能听到轻微啸叫；
	//   38 kHz 完全脱离听觉范围，同时仍在 1S 有刷 + MOS 直驱的甜点区内。
	//   为什么不用上游默认的 78000：那是"把 10-bit 分辨率顶到天花板"的结果
	//   （LEDC 时钟 80 MHz ÷ 1024 级 = 78.125 kHz，再高只能降分辨率），
	//   而频率超过约 40 kHz 后纹波收益急剧递减（纹波 ∝ 1/f），
	//   开关损耗（∝ f）与对 ADC/IMU 的开关噪声耦合却线性上涨 —— 顶格不划算。
	//   取舍依据：纹波 ΔI = V·D(1−D)/(L·f)，损耗 P ≈ V·I·(t_r+t_f)/2 × f，
	//   两条曲线在 20~40 kHz 之间交叉，38 kHz 取在甜点区中段。
	//
	//   🔴 只在火龙果分支里赋值：S3 不走这里，编译产物与 v1.1 保持字节级一致
	//      （改的是"另一个板型的默认值"，不是公共行为）。
	// -----------------------------------------------------------------------
	#if FLIX_BOARD == BOARD_HLG_V2
		pwmFrequency = 38000;
	#endif

	// -----------------------------------------------------------------------
	// 【改造说明】原来这里是 #ifdef CONFIG_IDF_TARGET_ESP32
	//   那段逻辑的问题是：只要编到经典 ESP32 就无条件把电机脚改成 12/13/14/15，
	//   而 setDefaults() 是在 setupParameters() 里**无条件调用**的，
	//   于是它会覆盖掉 motors.ino 里写的引脚 —— 宏写了也白写。
	//   对火龙果 2.0 板尤其致命：12/13/14/15 正好是 PMW3901 光流模块的四个脚，
	//   上电就等于拿光流模块的脚去推电机，四轴乱序起转。
	//
	//   现在改成按板型判断：只有"未指定板型的上游通用经典 ESP32"才套用这套历史默认值，
	//   其余板型（S3 / 火龙果）的电机脚完全由 board.h 提供。
	//   对耀坤 S3 板零影响：S3 编译时 CONFIG_IDF_TARGET_ESP32 本来就没定义，
	//   这段以前就不生效，现在同样不生效。
	// -----------------------------------------------------------------------
	#if FLIX_BOARD == BOARD_GENERIC_ESP32
		// classic esp32 configuration（仅上游通用板型，本产品线不使用）
		motorPins[MOT_RL] = 12;
		motorPins[MOT_RR] = 13;
		motorPins[MOT_FR] = 14;
		motorPins[MOT_FL] = 15;
	#endif

	#ifdef FLIX2
		imuModel = 4; // ICM-40609-D
		imuIntPin = 10;
		imuCsPin = 14;
		voltagePin = 3;
		motorPins[MOT_RL] = 41;
		motorPins[MOT_RR] = 7;
		motorPins[MOT_FR] = 18;
		motorPins[MOT_FL] = 38;
	#endif
}
