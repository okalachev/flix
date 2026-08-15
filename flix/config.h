// Copyright (c) 2026 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Parameter defaults

#pragma once

void setDefaults() {
	// Set defaults here

	#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3)
		pwmFrequency = 38000;
	#endif

	#ifdef CONFIG_IDF_TARGET_ESP32
		// classic esp32 configuration
		motorPins[0] = 12;
		motorPins[1] = 13;
		motorPins[2] = 14;
		motorPins[3] = 15;
	#endif

	#ifdef FLIX2
		imuModel = 4; // ICM-40609-D
		imuIntPin = 10;
		imuCsPin = 14;
		voltagePin = 3;
		motorPins[0] = 41;
		motorPins[1] = 7;
		motorPins[2] = 18;
		motorPins[3] = 38;
	#endif
}
