// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix
// 耀坤整理版修改：https://github.com/YaoKunS/flix

// ============================================================================
// 耀坤S3 飞控：RGB 灯语系统 + 蜂鸣器音频反馈
// ============================================================================
//
// 【硬件映射（耀坤 S3 立创版 PCB 实测）】
//   红灯   GPIO41   低电报警 / 失联慢闪
//   绿灯   GPIO7    已连接常亮 / 已解锁快闪
//   蓝灯   GPIO14   初始化过程指示
//   蜂鸣器 GPIO39   有源蜂鸣器，高电平触发（S8050 三极管驱动）
//
// 【灯语优先级（高 → 低）】
//   1. 低电量（电压 < 3.5 V） → 红灯 100 ms 快闪 + 蜂鸣器连续急鸣
//   2. 已连接 且 已解锁      → 绿灯 200 ms 快闪
//   3. 已连接 且 未解锁      → 绿灯常亮
//   4. 未连接                → 红灯 500 ms 慢闪
//   5. 初始化中              → 蓝灯常亮（由 setup() 直接控制）
//
// 【声音反馈（非阻塞播放器）】
//   'S' 短鸣   上电 / 初始化完成
//   'D' 双短鸣 遥控连接成功
//   'T' 三短鸣 解锁成功
//   'L' 长鸣   失去遥控
//   'C' 连续急鸣 低电量（循环直到电压恢复）
//   'X' 立即停止
//
// 【为什么写非阻塞播放器】
//   如果在 setup()/loop() 里用 delay() 放音，1 kHz 的主循环会被直接打断，
//   姿态解算跟着丢步 ⇒ 解锁瞬间就翻。所以这里用"状态机 + 由主循环驱动"：
//   任何时刻都不阻塞，只有 setup() 里那两次"开始/完成"提示音会用
//   runBuzzerFor() 主动跑一小段（此时还没解锁，阻塞无害）。
//
// 【与自研飞控 YaoKunDrone 的一致性】
//   低电阈值同样取 3.5 V（1S 锂电），音效语义保持"短/双/三/长/急"，
//   让两套飞控的操作反馈手感一致，用户不用重新学。
// ============================================================================

#include "util.h"
#include "i18n.h"   // 界面文字唯一数据源：中/英文可切（FLIX_LANG_EN）

extern float t;
extern float mavlinkTime;   // 安全层：最后收到遥控的时刻
extern bool armed;
extern float voltage;
extern float controlTime;   // MANUAL_CONTROL 的时间戳（见下方 connected 判定说明）

// ---- 引脚与阈值（耀坤 S3 实测；已接入参数表，可用 `p LED_R_PIN ...` 现场改）----
// 【为什么用变量而不是 #define】参数表 parameters[] 需要变量指针才能读写；
// 用宏就没有地址。这里定义成全局变量后，LED_* / BUZ_* 参数可直接映射到它们，
// 改引脚不必重编固件（换板/飞线调试时非常有用）。
int ledRedPin = 41;        // 红灯：低电报警 / 失联慢闪
int ledGreenPin = 7;       // 绿灯：已连接常亮 / 已解锁快闪
int ledBluePin = 14;       // 蓝灯：初始化过程指示
int buzzerPin = 39;        // 有源蜂鸣器，高电平触发（S8050 三极管驱动）
int buzzerEnable = 1;      // 蜂鸣器总开关：0 = 全程静音（夜间室内调试用）
float ledLowVoltage = 3.5f; // 低电告警阈值（1S 锂电，与自研飞控 YaoKunDrone 一致）

// ---- LED 状态缓存（避免重复写 IO）----
static bool redState = false, greenState = false, blueState = false;

// ============================================================================
// LED 独立控制（带状态缓存）
// ============================================================================
void setRed(bool on) {
	if (on != redState) {
		digitalWrite(ledRedPin, on ? HIGH : LOW);
		redState = on;
	}
}

void setGreen(bool on) {
	if (on != greenState) {
		digitalWrite(ledGreenPin, on ? HIGH : LOW);
		greenState = on;
	}
}

void setBlue(bool on) {
	if (on != blueState) {
		digitalWrite(ledBluePin, on ? HIGH : LOW);
		blueState = on;
	}
}

// 兼容上游代码（只控制蓝灯）
void setLED(bool on) {
	setBlue(on);
}

// ============================================================================
// 非阻塞蜂鸣器播放器
// ============================================================================
static unsigned long buzzerStartTime = 0; // 当前阶段起始时刻（ms）
static int buzzerPhase = 0;               // 0=空闲, 1=发声, 2=静音间隔
static int buzzerDurationOn = 0;          // 发声时长（ms）
static int buzzerDurationOff = 0;         // 静音时长（ms）
static int buzzerRepeatCount = 0;         // 剩余重复次数（-1 = 无限）
static bool buzzerInfinite = false;       // 是否无限循环

/**
 * 触发一段蜂鸣音效（非阻塞）
 * @param pattern 音效类型：'S' 短鸣 / 'L' 长鸣 / 'D' 双短鸣 / 'T' 三短鸣
 *                'C' 连续急鸣（无限） / 'X' 立即停止
 * 调用时机：状态变化时调用一次即可，之后由 buzzerUpdate() 在后台推进。
 */
void buzzerPlay(char pattern) {
	buzzerInfinite = false;
	if (!buzzerEnable) { // 蜂鸣器被参数关掉：只清状态，不驱动 IO
		buzzerPhase = 0;
		return;
	}
	switch (pattern) {
		case 'S': // 短鸣
			buzzerDurationOn = 120;
			buzzerDurationOff = 100;
			buzzerRepeatCount = 1;
			break;
		case 'L': // 长鸣
			buzzerDurationOn = 500;
			buzzerDurationOff = 200;
			buzzerRepeatCount = 1;
			break;
		case 'D': // 双短鸣
			buzzerDurationOn = 120;
			buzzerDurationOff = 100;
			buzzerRepeatCount = 2;
			break;
		case 'T': // 三短鸣
			buzzerDurationOn = 120;
			buzzerDurationOff = 100;
			buzzerRepeatCount = 3;
			break;
		case 'C': // 连续急鸣（无限循环）
			buzzerDurationOn = 150;
			buzzerDurationOff = 150;
			buzzerRepeatCount = -1;
			buzzerInfinite = true;
			break;
		case 'X': // 停止
		default:
			buzzerPhase = 0;
			digitalWrite(buzzerPin, LOW);
			return;
	}
	// 立即开始发声
	buzzerPhase = 1;
	buzzerStartTime = millis();
	digitalWrite(buzzerPin, HIGH);
}

/**
 * 蜂鸣器状态推进（必须由主循环高频调用）
 * 处理"发声 → 静音 → 下一次发声"的时序。
 */
static void buzzerUpdate() {
	if (buzzerPhase == 0) return; // 空闲

	const unsigned long now = millis();
	if (buzzerPhase == 1) { // 正在发声
		if (now - buzzerStartTime >= (unsigned long)buzzerDurationOn) {
			digitalWrite(buzzerPin, LOW);
			buzzerPhase = 2;
			buzzerStartTime = now;
			// 有限音效且已是最后一次 → 直接结束
			if (!buzzerInfinite && buzzerRepeatCount == 1) {
				buzzerPhase = 0;
				buzzerRepeatCount = 0;
			}
		}
	} else if (buzzerPhase == 2) { // 静音间隔
		if (now - buzzerStartTime >= (unsigned long)buzzerDurationOff) {
			if (buzzerInfinite) {
				buzzerPhase = 1;
				buzzerStartTime = now;
				digitalWrite(buzzerPin, HIGH);
				return;
			}
			if (buzzerRepeatCount > 0) buzzerRepeatCount--;
			if (buzzerRepeatCount == 0) {
				buzzerPhase = 0;
				digitalWrite(buzzerPin, LOW);
			} else {
				buzzerPhase = 1;
				buzzerStartTime = now;
				digitalWrite(buzzerPin, HIGH);
			}
		}
	}
}

/**
 * 阻塞式跑一段蜂鸣器（仅供 setup() 使用）
 *
 * 【为什么 setup 里可以阻塞】此时还没解锁、电机没转，阻塞几百毫秒无副作用，
 * 反而能让"开机提示音"完整播完再往下走，用户体验更清楚。
 * 主循环里**绝对不要**调用它。
 */
void runBuzzerFor(unsigned long ms) {
	const unsigned long start = millis();
	while (millis() - start < ms) {
		buzzerUpdate();
		delay(1);
	}
}

// ============================================================================
// 初始化
// ============================================================================
void setupLED() {
	pinMode(ledRedPin, OUTPUT);
	pinMode(ledGreenPin, OUTPUT);
	pinMode(ledBluePin, OUTPUT);
	pinMode(buzzerPin, OUTPUT);
	digitalWrite(ledRedPin, LOW);
	digitalWrite(ledGreenPin, LOW);
	digitalWrite(ledBluePin, LOW);
	digitalWrite(buzzerPin, LOW);
	// 同步缓存，避免首次 setXxx(true) 被"状态未变"逻辑吞掉
	redState = greenState = blueState = false;
}

// ============================================================================
// 灯语 + 音频主状态机（主循环每帧调用）
// ============================================================================

// 状态编码：-2=低电量, 0=未连接, 1=已连接未解锁, 2=已连接已解锁
static int lastBuzzerState = -1;
static bool buzzerLowBatteryStarted = false;

void updateLED() {
	// 1. 先推进蜂鸣器时序
	buzzerUpdate();

	// 2. 低电量报警（最高优先级）
	//    阈值来自参数 LED_LOW_VOLT（出厂 3.5 V，与自研飞控 YaoKunDrone 保持一致，1S 锂电）
	const bool lowBattery = valid(voltage) && (voltage < ledLowVoltage);
	if (lowBattery) {
		const uint32_t now = millis();
		setRed((now / 50) % 2); // 红灯 100 ms 快闪
		setGreen(false);
		setBlue(false);
		if (!buzzerLowBatteryStarted) {
			buzzerPlay('C');
			buzzerLowBatteryStarted = true;
		}
		lastBuzzerState = -2;
		return;
	}
	if (buzzerLowBatteryStarted) { // 刚退出低电量
		buzzerPlay('X');
		buzzerLowBatteryStarted = false;
	}

	// 3. 连接判定
	//    【为什么不直接用 mavlinkTime】上游任何 MAVLink 帧都会刷新 mavlinkTime，
	//    包括飞机自己发出去的包被回环计数、以及 GCS 的心跳。结果就是"没接遥控
	//    也显示已连接"，灯语骗人。这里改用 controlTime —— 它只在
	//    MANUAL_CONTROL（遥控摇杆帧）到达时更新，语义上才是"遥控在线"。
	//    若启用了 SBUS，readRC() 也会更新 controlTime，两条路径都对。
	const bool connected = valid(controlTime) && (t - controlTime < 1.0f);

	// 4. 状态编码
	const int currentState = connected ? (armed ? 2 : 1) : 0;

	// 5. 状态变化 → 触发一次音效（跳过上电后的首次状态）
	if (currentState != lastBuzzerState && lastBuzzerState != -1) {
		switch (currentState) {
			case 0: buzzerPlay('L'); break; // 失联：长鸣
			case 1: buzzerPlay('D'); break; // 连接成功：双短鸣
			case 2: buzzerPlay('T'); break; // 解锁成功：三短鸣
			default: break;
		}
	}
	lastBuzzerState = currentState;

	// 6. 灯语
	const uint32_t now = millis();
	if (connected && armed) {
		setGreen((now / 100) % 2); // 已解锁：绿灯快闪
		setRed(false);
		setBlue(false);
	} else if (connected) {
		setGreen(true); // 未解锁：绿灯常亮
		setRed(false);
		setBlue(false);
	} else {
		setRed((now / 250) % 2); // 未连接：红灯慢闪
		setGreen(false);
		setBlue(false);
	}
}
