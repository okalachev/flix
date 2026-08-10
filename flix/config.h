// Copyright (c) 2026 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Parameter defaults

#pragma once

void setDefaults() {
	// Set defaults here

	#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3)
		pwmFrequency = 38000;
	#endif

	#ifdef FLIX2
		imuModel = 4; // ICM-40609-D
		imuIntPin = 10;
		imuCsPin = 14;

		motorPins[0] = 41;
		motorPins[1] = 7;
		motorPins[2] = 38;
		motorPins[3] = 18;

		voltagePin = 3;
	#endif
}
