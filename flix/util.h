// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Utility functions

#pragma once

#include <math.h>
#include <ESP32_NOW_Serial.h>
#include <Preferences.h>

const float ONE_G = 9.80665;
extern float t;

#define STRINGIFY(x) __STRINGIFY(x)
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

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

// Split string by whitespace
template <size_t TOKENS>
void split(const String& str, String (&arr)[TOKENS]) {
	size_t pos = 0;
	const size_t length = str.length();
	for (size_t i = 0; i < TOKENS; i++) {
		while (pos < length && isspace((unsigned char)str[pos])) pos++;
		size_t start = pos;
		while (pos < length && !isspace((unsigned char)str[pos])) pos++;
		arr[i] = str.substring(start, pos);
	}
}

// Read a string from Preferences without printing error if the key doesn't exist
String getString(Preferences& preferences, const char* key, const char* default_) {
	if (!preferences.isKey(key)) return default_;
	return preferences.getString(key, default_);
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

// Variant type for logging and parameters
struct Value {
	enum { EMPTY, FLOAT, INT, BOOL, FLOAT_FN, INT_FN, BOOL_FN } type;
	union {
		void *pointer;
		float *float_;
		int *int_;
		bool *bool_;
		float (*floatFn)();
		int (*intFn)();
		bool (*boolFn)();
	};

	Value() : type(EMPTY), pointer(nullptr) {};
	Value(float *pt) : type(FLOAT), float_(pt) {};
	Value(int *pt) : type(INT), int_(pt) {};
	Value(bool *pt) : type(BOOL), bool_(pt) {};
	Value(float (*fn)()) : type(FLOAT_FN), floatFn(fn) {};
	Value(int (*fn)()) : type(INT_FN), intFn(fn) {};
	Value(bool (*fn)()) : type(BOOL_FN), boolFn(fn) {};

	float get() const {
		switch (type) {
			case FLOAT: return *float_;
			case INT: return *int_;
			case BOOL: return *bool_ ? 1 : 0;
			case FLOAT_FN: return floatFn();
			case INT_FN: return intFn();
			case BOOL_FN: return boolFn() ? 1 : 0;
			default: return NAN;
		}
	};

	bool set(float value) const {
		switch (type) {
			case FLOAT: *float_ = value; break;
			case INT: if (!isfinite(value)) return false; *int_ = value; break;
			case BOOL: *bool_ = (value != 0); break;
			default: return false;
		}
		return true;
	}
};

// Rate limiter
class Rate {
public:
	float rate;
	float last = -INFINITY;
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
