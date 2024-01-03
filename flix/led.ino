// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Main LED control

#define BLINK_PERIOD 500000

#ifndef LED_BUILTIN
#define LED_BUILTIN 2 // for ESP32 Dev Module
#endif

void setupLED() {
	pinMode(LED_BUILTIN, OUTPUT);
}

void setLED(bool on) {
	digitalWrite(LED_BUILTIN, on ? HIGH : LOW);
}

void blinkLED() {
	setLED(micros() / BLINK_PERIOD % 2);
}
