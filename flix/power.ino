// Copyright (c) 2026 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Power management

#ifdef ESP32
#include <soc/soc.h>
#include <soc/rtc_cntl_reg.h>
#endif
#include "lpf.h"
#include "filter.h"
#include "util.h"

float voltage = NAN;
LowPassFilter<float> voltageFilter(1);
int voltagePin = -1;
float voltageScale = 2;

void setupPower() {
#ifdef ESP32
	REG_CLR_BIT(RTC_CNTL_BROWN_OUT_REG, RTC_CNTL_BROWN_OUT_ENA); // disable reset on low voltage
	if (digitalPinToAnalogChannel(voltagePin) == -1) voltagePin = -1; // test ADC pin
#endif
}

void readVoltage() {
	if (voltagePin < 0) return;

	static Rate rate(10);
	if (!rate) return;

	float v = 0;
#if defined(ESP32)
	v = analogReadMilliVolts(voltagePin) * voltageScale / 1000.0f;
#elif defined(ARDUINO_ARCH_STM32)
	v = analogRead(voltagePin) * voltageScale * 3.3f / 4095.0f;
#endif

	voltage = voltageFilter.update(v);
}
