// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Utility functions

#pragma once

#include <math.h>
#include <ESP32_NOW_Serial.h>

const float ONE_G = 9.80665;
extern float t;

float mapf(float x, float in_min, float in_max, float out_min, float out_max) {
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

bool invalid(float x) {
	return !isfinite(x);
}

bool valid(float x) {
	return isfinite(x);
}

bool floatEquals(float a, float b, float epsilon = 0) {
	if (isnan(a) && isnan(b)) return true;
	if (a == b) return true;
	return fabsf(a - b) <= epsilon;
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

// Trim and split string by spaces
void splitString(String& str, String& token0, String& token1, String& token2) {
	str.trim();
	if (str.isEmpty()) return;
	char chars[str.length() + 1];
	str.toCharArray(chars, str.length() + 1);
	token0 = strtok(chars, " ");
	token1 = strtok(NULL, " ");
	token2 = strtok(NULL, "");
	if (token1.c_str() == NULL) token1 = "";
	if (token2.c_str() == NULL) token2 = "";
}

// Simplified ESP-NOW Serial without resends
class ESPNOWSerial : public ESP_NOW_Serial_Class {
public:
	int lost = 0;
	using ESP_NOW_Serial_Class::ESP_NOW_Serial_Class;
	void onSent(bool success) override {
		if (!success) lost++;
		ESP_NOW_Serial_Class::onSent(true); // always report success to avoid resends
	}
};

// Simple variant type for logging and parameters
struct Value {
	enum { EMPTY, FLOAT, INT, BOOL, FLOAT_FN, INT_FN, BOOL_FN } type;
	union {
		void *pointer;
		float *_float;
		int *_int;
		bool *_bool;
		float (*floatFn)();
		int (*intFn)();
		bool (*boolFn)();
	};

	Value() : type(EMPTY), pointer(nullptr) {};
	Value(float *pt) : type(FLOAT), _float(pt) {};
	Value(int *pt) : type(INT), _int(pt) {};
	Value(bool *pt) : type(BOOL), _bool(pt) {};
	Value(float (*fn)()) : type(FLOAT_FN), floatFn(fn) {};
	Value(int (*fn)()) : type(INT_FN), intFn(fn) {};
	Value(bool (*fn)()) : type(BOOL_FN), boolFn(fn) {};

	float get() const {
		switch (type) {
			case FLOAT: return *_float;
			case INT: return *_int;
			case BOOL: return *_bool ? 1 : 0;
			case FLOAT_FN: return floatFn();
			case INT_FN: return intFn();
			case BOOL_FN: return boolFn() ? 1 : 0;
			default: return NAN;
		}
	};

	void set(float value) const {
		switch (type) {
			case FLOAT: *_float = value; break;
			case INT: *_int = value; break;
			case BOOL: *_bool = (value != 0); break;
			default: break;
		}
	};
};

// Rate limiter
class Rate {
public:
	float rate;
	float last = 0;
	Rate(float rate) : rate(rate) {}

	operator bool() {
		if (t == last) {
			return true; // the same step
		}
		if (t - last >= 1 / rate) {
			last = t;
			return true;
		}
		return false;
	}
};

// Delay filter for boolean signals - ensures the signal is on for at least 'delay' seconds
class Delay {
public:
	float delay;
	float start = NAN;
	Delay(float delay) : delay(delay) {}

	bool update(bool on) {
		if (!on) {
			start = NAN;
			return false;
		} else if (isnan(start)) {
			start = t;
		}
		return t - start >= delay;
	}
};
