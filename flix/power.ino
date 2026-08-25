// Copyright (c) 2026 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Power management

#include "esp_private/brownout.h"
#include "filter.h"
#include "util.h"

float voltage = NAN;
LowPassFilter<float> voltageFilter(1);
int voltagePin = -1;
float voltageScale = 2;

void setupPower() {
	esp_brownout_disable(); // disable reset on low voltage
	if (digitalPinToAnalogChannel(voltagePin) == -1) voltagePin = -1; // test ADC pin
}

void readVoltage() {
	if (voltagePin < 0) return;

	static Rate rate(10);
	if (!rate) return;

	float v = analogReadMilliVolts(voltagePin) * voltageScale / 1000.0f;
	voltage = voltageFilter.update(v);
}
