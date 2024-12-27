// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Utility functions

#include <math.h>
#include <soc/soc.h>
#include <soc/rtc_cntl_reg.h>

float mapf(long x, long in_min, long in_max, float out_min, float out_max) {
	return (float)(x - in_min) * (out_max - out_min) / (float)(in_max - in_min) + out_min;
}

float mapff(float x, float in_min, float in_max, float out_min, float out_max) {
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// Wrap angle to [-PI, PI)
float wrapAngle(float angle) {
	angle = fmodf(angle, 2 * PI);
	if (angle > PI) {
		angle -= 2 * PI;
	} else if (angle < -PI) {
		angle += 2 * PI;
	}
	return angle;
}

template <typename T>
void printArray(T arr[], int size) {
	for (uint8_t i = 0; i < size; i++) {
		Serial.printf("%g", static_cast<float>(arr[i]));
		if (i < size - 1) Serial.print(" ");
	}
	Serial.println();
}

// Disable reset on low voltage
void disableBrownOut() {
	WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
}
