// Copyright (c) 2026 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Power management

#include "lpf.h"
#include "util.h"

float voltage;
LowPassFilter<float> voltageFilter(0.2);
int voltagePin = -1;
float voltageScale = 2;

void readVoltage() {
	if (voltagePin < 0) return;
	static Rate rate(10);
	if (!rate) return;

	float v = analogReadMilliVolts(voltagePin) * voltageScale / 1000.0f;
	voltage = voltageFilter.update(v);
}
