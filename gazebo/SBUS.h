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
	bool read() { return joystickInit(); };
	SBUSData data() {
		SBUSData data;
		joystickGet(data.ch);
		for (int i = 0; i < 16; i++) {
			data.ch[i] = map(data.ch[i], -32768, 32767, 1000, 2000); // convert to pulse width style
		}
		return data;
	};
};
