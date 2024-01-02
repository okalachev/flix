// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// SBUS library mock to make it possible to compile simulator with rc.ino

#include "joystick.h"

class SBUS {
public:
	SBUS(HardwareSerial& bus) {};
	void begin() {};
	bool read(int16_t* channels, bool* failsafe, bool* lostFrame) { joystickGet(); return true; }; // NOTE: on the hardware channels is uint16_t
};
