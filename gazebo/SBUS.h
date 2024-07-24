// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// SBUS library mock to make it possible to compile simulator with rc.ino

#include "joystick.h"

struct SBUSData {
	int16_t ch[16];
};

class SBUS {
public:
	SBUS(HardwareSerial& bus, const bool inv = true) {};
	SBUS(HardwareSerial& bus, const int8_t rxpin, const int8_t txpin, const bool inv = true) {};
	void begin() {};
	bool read() { return joystickGet(); };
	SBUSData data() {
		SBUSData data;
		for (uint8_t i = 0; i < 16; i++) {
			data.ch[i] = channels[i];
		}
		return data;
	};
};
