// Copyright (c) 2026 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix
// 耀坤整理版修改：https://github.com/YaoKunS/flix

// 电源与电压检测
//
// 【耀坤 S3 硬件】电池分压接在 GPIO2（ADC2_CH1），实测分压比约 1:2。
//   上游默认 voltagePin = -1（禁用）⇒ 没有电压显示、没有低电报警，
//   对 1S 锂电来说是很危险的默认值（3.5 V 以下继续飞会过放损坏电池）。
//
// 【为什么必须关掉 brownout】上游已经关了（REG_CLR_BIT）、这里保留：
//   1S 锂电 + ME6217 LDO 的压差余量很小，起飞瞬间电机浪涌会把 3.3 V 拉塌，
//   硬件欠压复位会让飞机在空中重启 —— 这是必炸机级别的故障。

#include <soc/soc.h>
#include <soc/rtc_cntl_reg.h>
#include "filter.h"
#include "util.h"
#include "i18n.h"
#include "board.h"   // 板型引脚表：PIN_VOLTAGE

float voltage = NAN;
LowPassFilter<float> voltageFilter(1);
// 【引脚来自 board.h】S3 板 = GPIO2，火龙果 2.0 = GPIO35。
// 🔴 换板型时这个脚必须落在 **ADC1**：
//    - ESP32-S3：GPIO2 = ADC1_CH1（S3 板的 2 号脚本来就在 ADC1，安全）
//    - 经典 ESP32：GPIO35 = ADC1_CH7（火龙果板，安全）
//    经典 ESP32 的 ADC2（0/2/4/12~15/25~27）与 Wi-Fi 互斥，而本固件热点常开
//    ⇒ ADC2 读数恒 0 ⇒ 判定低电 ⇒ 蜂鸣器无限急鸣。加新板型前务必先查 ADC 分组。
int voltagePin = PIN_VOLTAGE;   // 电池电压分压输入
float voltageScale = 2;    // 分压比 1:2 ⇒ 读数乘 2

void setupPower() {
	print(TXT_SETUP_POWER);
	REG_CLR_BIT(RTC_CNTL_BROWN_OUT_REG, RTC_CNTL_BROWN_OUT_ENA); // 关闭欠压复位（原因见文件头）
	if (digitalPinToAnalogChannel(voltagePin) == -1) {
		// 引脚不是 ADC 口时明确告警：否则用户会以为"电压一直是 nan"是别的原因
		print(TXT_VOLT_PIN_ERR, voltagePin);
		voltagePin = -1;
		return;
	}
	// 打出实际生效的引脚与分压系数 —— 换板子后一眼就能确认参数有没有生效
	print(TXT_VOLT_PIN_OK, voltagePin, voltageScale);
}

void readVoltage() {
	if (voltagePin < 0) return;

	static Rate rate(10);
	if (!rate) return;

	float v = analogReadMilliVolts(voltagePin) * voltageScale / 1000.0f;
	voltage = voltageFilter.update(v);
}
