// Copyright (c) 2026 YaoKunS
// Repository: https://github.com/YaoKunS/flix
//
// ============================================================================
// LED 板型映射（三色灯 / 单灯）
// ============================================================================
//
// 【为什么这些宏必须放在 .h 里，而不能直接写在 led.ino 里】
// 🔴 这是踩了三次坑换来的规则，务必遵守：
//   Arduino 编译 .ino 时用 ctags 扫描函数定义、自动生成函数原型。
//   实测发现，只要 .ino 里出现 **#if 包住一整段代码**（尤其是含多行反斜杠
//   续行宏的块），ctags 就会解析错位；一旦错位，**整个合并文件**的函数原型
//   全部不生成，编译报几十条与本处毫不相关的
//   "setupParameters / setupMotors / updateLED was not declared in this scope"。
//   把这段条件代码搬进 .h 就没事 —— Arduino 不对 .h 跑 ctags。
//   ⇒ 规则：板型差异写在 .h 里，.ino 里只保留普通函数和一行宏调用。
//
// 【单灯方案的核心结论】
//   updateLED() 的四个分支（低电 / 已解锁 / 已连接 / 未连接）每个都显式把
//   另外两路灯置 false，所以任一时刻最多只有一路活动。
//   ⇒ 单灯只要取三路的 OR 即可，**状态机一行都不用改**，
//     闪频天然继承：低电 10 Hz / 已解锁 5 Hz / 未连接 2 Hz / 已连接常亮。
//
// 【三色板为什么写成"与改造前逐字符相同"的宏】
//   验收标准是「耀坤 S3 板的编译产物 sha256 与 v1.1 基线一致」。
//   宏展开后与改造前手写的 digitalWrite 语句完全等价，才可能做到零差异。
// ============================================================================

#pragma once

#include "board.h"   // BOARD_LED_COUNT / PIN_LED_* / PIN_BUZZER

// ---------------------------------------------------------------------------
// 判定：这块板是不是只有一颗灯
// ---------------------------------------------------------------------------
// 条件写两个是为了双保险：板型表明确声明单灯，或者绿/蓝引脚无效。
#if (BOARD_LED_COUNT == 1) || ((PIN_LED_G < 0) && (PIN_LED_B < 0))
	#define LED_SINGLE_CHANNEL 1
#else
	#define LED_SINGLE_CHANNEL 0
#endif

#if LED_SINGLE_CHANNEL

	// ===== 单灯板 =====
	// 那一颗灯当前的物理电平（避免重复写 IO）。
	// 【为什么定义在头文件里】用 #if 包住，三色板就完全没有这个变量，
	// 保证 RAM 占用与改造前一致。定义成 static 只在 led.ino 内部可见。
	static bool redPinLevel = false;

	// 合成：任一路亮则点亮。
	#define LED_RENDER() do { \
		const bool lvl_ = redState || greenState || blueState; \
		if (lvl_ != redPinLevel) { \
			digitalWrite(ledRedPin, lvl_ ? HIGH : LOW); \
			redPinLevel = lvl_; \
		} \
	} while (0)

	#define LED_COMMIT_RED(on)   do { redState   = (on); LED_RENDER(); } while (0)
	#define LED_COMMIT_GREEN(on) do { greenState = (on); LED_RENDER(); } while (0)
	#define LED_COMMIT_BLUE(on)  do { blueState  = (on); LED_RENDER(); } while (0)

	// 单灯板必须逐路判空：绿/蓝是 -1，直接 pinMode(-1) 不崩但会打印
	// "Invalid pin"，把串口日志弄脏、掩盖真正的错误。
	#define LED_SETUP_PINS() do { \
		if (ledRedPin   >= 0) { pinMode(ledRedPin, OUTPUT);   digitalWrite(ledRedPin, LOW); } \
		if (ledGreenPin >= 0) { pinMode(ledGreenPin, OUTPUT); digitalWrite(ledGreenPin, LOW); } \
		if (ledBluePin  >= 0) { pinMode(ledBluePin, OUTPUT);  digitalWrite(ledBluePin, LOW); } \
		if (buzzerPin   >= 0) { pinMode(buzzerPin, OUTPUT);   digitalWrite(buzzerPin, LOW); } \
		redState = greenState = blueState = false; \
		redPinLevel = false; \
	} while (0)

#else

	// ===== 三色板 =====
	// 下面这些宏展开后与改造前手写的代码**逐字符相同**，
	// 这是「sha256 与 v1.1 基线一致」的前提。改这里之前请先想清楚。
	#define LED_COMMIT_RED(on)   do { if ((on) != redState)   { digitalWrite(ledRedPin,   (on) ? HIGH : LOW); redState   = (on); } } while (0)
	#define LED_COMMIT_GREEN(on) do { if ((on) != greenState) { digitalWrite(ledGreenPin, (on) ? HIGH : LOW); greenState = (on); } } while (0)
	#define LED_COMMIT_BLUE(on)  do { if ((on) != blueState)  { digitalWrite(ledBluePin,  (on) ? HIGH : LOW); blueState  = (on); } } while (0)

	// 三色板无条件配置四路引脚（与改造前一致）。
	#define LED_SETUP_PINS() do { \
		pinMode(ledRedPin, OUTPUT); \
		pinMode(ledGreenPin, OUTPUT); \
		pinMode(ledBluePin, OUTPUT); \
		pinMode(buzzerPin, OUTPUT); \
		digitalWrite(ledRedPin, LOW); \
		digitalWrite(ledGreenPin, LOW); \
		digitalWrite(ledBluePin, LOW); \
		digitalWrite(buzzerPin, LOW); \
		redState = greenState = blueState = false; \
	} while (0)

#endif

// ---------------------------------------------------------------------------
// 蜂鸣器电平输出（编译期宏，零运行时开销）
// ---------------------------------------------------------------------------
// 【为什么用宏而不是函数】函数调用/内联会带来几十字节的产物差异，
// 而三色板要求与改造前完全一致。宏展开的结果与手写的 digitalWrite 等价。
// 【为什么要判 PIN_BUZZER >= 0】某些板型可能不装蜂鸣器，此时展开为空操作，
// 避免对 -1 号脚做 digitalWrite（未定义行为）。
// 当前两个板型都有蜂鸣器（S3 = 39 / 火龙果 = 25）。
#if (PIN_BUZZER >= 0)
	#define SET_BUZZER_LEVEL(on) digitalWrite(buzzerPin, (on) ? HIGH : LOW)
#else
	#define SET_BUZZER_LEVEL(on) ((void)0)
#endif
