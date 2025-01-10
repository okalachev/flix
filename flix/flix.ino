// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Main firmware file

#include "vector.h"
#include "quaternion.h"
#include "util.h"

#define SERIAL_BAUDRATE 115200
#define WIFI_ENABLED 1

#define RC_CHANNELS 16
#define RC_CHANNEL_ROLL 0
#define RC_CHANNEL_PITCH 1
#define RC_CHANNEL_THROTTLE 2
#define RC_CHANNEL_YAW 3
#define RC_CHANNEL_ARMED 4
#define RC_CHANNEL_MODE 5

float t = NAN; // current step time, s
float dt; // time delta from previous step, s
int16_t channels[RC_CHANNELS]; // raw rc channels
float controls[RC_CHANNELS]; // normalized controls in range [-1..1] ([0..1] for throttle)
float controlsTime; // time of the last controls update
Vector gyro; // gyroscope data
Vector acc; // accelerometer data, m/s/s
Vector rates; // filtered angular rates, rad/s
Quaternion attitude; // estimated attitude
float motors[4]; // normalized motors thrust in range [-1..1]

void setup() {
	Serial.begin(SERIAL_BAUDRATE);
	Serial.println("Initializing flix");
	disableBrownOut();
	setupParameters();
	setupLED();
	setupMotors();
	setLED(true);
#if WIFI_ENABLED == 1
	setupWiFi();
#endif
	setupIMU();
	setupRC();

	setLED(false);
	Serial.println("Initializing complete");
}

void loop() {
	readIMU();
	step();
	readRC();
	estimate();
	control();
	sendMotors();
	parseInput();
#if WIFI_ENABLED == 1
	processMavlink();
#endif
	logData();
	syncParameters();
}
