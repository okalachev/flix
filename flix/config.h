// Copyright (c) 2026 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Board config and parameter defaults

#pragma once

// Uncomment this to build for certain target in Arduino IDE
// #define FLIX2


#ifdef FLIX2
#define RGB_BUILTIN 21
#ifndef CONFIG_IDF_TARGET_ESP32S3
#error "Set Tools -> Board to ESP32S3 Dev Module for Flix2 board"
#endif
#ifndef CONFIG_SPIRAM_MODE_OCT
#error "Set Tools -> PSRAM to OPI for Flix2 board"
#endif
#endif

extern const int RAW, ACRO, STAB, AUTO;

void setDefaults() {
	// Set defaults here

	#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3)
		pwmFrequency = 38000;
	#endif

	#ifdef CONFIG_IDF_TARGET_ESP32
		// classic esp32 configuration
		motorPins[MOT_RL] = 12;
		motorPins[MOT_RR] = 13;
		motorPins[MOT_FR] = 14;
		motorPins[MOT_FL] = 15;
	#endif

	#ifdef RGB_BUILTIN
		ledType = 1; // WS2812
		ledPin = RGB_BUILTIN;
	#endif

	#ifdef FLIX2
		imuModel = 4; // ICM-40609-D
		imuIntPin = 10;
		imuCsPin = 14;
		voltagePin = 3;
		motorPins[MOT_RL] = 41;
		motorPins[MOT_RR] = 7;
		motorPins[MOT_FR] = 18;
		motorPins[MOT_FL] = 38;
	#endif

	if (ledType == 1) {
		// Default animations for LED strip
		modeAnimation[RAW] = packAnimation(FILL, 255, 0, 0);
		modeAnimation[ACRO] = packAnimation(BLINK, 0, 255, 0);
		modeAnimation[STAB] = packAnimation(RAINBOW, 0, 0, 255);
		modeAnimation[AUTO] = packAnimation(FILL, 255, 255, 255);
	}
}
