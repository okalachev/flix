// Copyright (c) 2026 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Board config and parameter defaults

#pragma once

// Uncomment to build for certain target in Arduino IDE:
// #define FLIX2
// #define AXEL

#ifndef VERSION
#define VERSION "unknown"
#endif

#if CONFIG_IDF_TARGET_ESP32 && !defined(LED_BUILTIN)
#define LED_BUILTIN 2 // use most common LED pin for ESP32 boards
#endif

#ifdef FLIX2
#define RGB_BUILTIN 21 + SOC_GPIO_PIN_COUNT
#ifndef CONFIG_IDF_TARGET_ESP32S3
#error "Set Tools -> Board to ESP32S3 Dev Module for Flix2 board"
#endif
#ifndef CONFIG_SPIRAM_MODE_OCT
#error "Set Tools -> PSRAM to OPI for Flix2 board"
#endif
#endif

#if CONFIG_IDF_TARGET_ESP32S3 && !defined(ARDUINO_USB_CDC_ON_BOOT)
#error "Set Tools -> USB CDC on Boot to Enabled"
#endif

#include "filter.h"
#include "vector.h"

// Declare configuration variables
extern int imuModel, imuIntPin, imuCsPin, ledType, ledPin, ledCount;
extern Vector imuRotation;
extern int motorPins[4];
extern int pwmFrequency, pwmResolution, pwmStop, pwmMin, pwmMax;
extern int rollChannel, pitchChannel, throttleChannel, yawChannel, armedChannel, modeChannel;
extern int channelZero[16], channelMax[16];
extern int rcRxPin, voltagePin;
extern int wifiMode, wifiLongRange, wifiBroadcast, udpLocalPort, udpRemotePort, espnowChannel;
extern float rcLossTimeout, descendTime, disarmTilt, voltageScale, voltageLow;
extern LowPassFilter<float> voltageFilter;
extern const int MOT_RL, MOT_RR, MOT_FR, MOT_FL;
extern const int W_AP, W_STA, W_ESPNOW;

void setupConfig() {
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
		// all boards with built-in RGB LED
		ledType = 1; // WS2812
		ledPin = RGB_BUILTIN - SOC_GPIO_PIN_COUNT;
	#endif

	#ifdef FLIX2
		imuModel = 4; // ICM-40609-D
		imuIntPin = 10;
		imuCsPin = 14;
		imuRotation = Vector(0, 0, -PI / 2);
		voltagePin = 3;
		motorPins[MOT_RL] = 18;
		motorPins[MOT_RR] = 38;
		motorPins[MOT_FR] = 41;
		motorPins[MOT_FL] = 7;
	#endif

	#ifdef AXEL
		imuModel = 1; // MPU-6500
		imuRotation = Vector(0, 0, 0);
		motorPins[MOT_RL] = 10;
		motorPins[MOT_RR] = 11;
		motorPins[MOT_FR] = 12;
		motorPins[MOT_FL] = 13;
		voltagePin = 5;
		ledType = 1; // WS2812
		ledPin = 6;
		ledCount = 68;
		standbyEffect = EFFECT_AXEL;
	#endif
}
