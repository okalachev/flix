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

		motorPins[MOTOR_REAR_LEFT] = 41;
		motorPins[MOTOR_REAR_RIGHT] = 7;
		motorPins[MOTOR_FRONT_RIGHT] = 18;
		motorPins[MOTOR_FRONT_LEFT] = 38;

		voltagePin = 3;
	#endif
}
