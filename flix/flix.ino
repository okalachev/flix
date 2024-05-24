// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Main firmware file

#include "vector.h"
#include "quaternion.h"

#define SERIAL_BAUDRATE 115200

#define WIFI_ENABLED 0

#define RC_CHANNELS 6
#define RC_CHANNEL_ROLL 0
#define RC_CHANNEL_PITCH 1
#define RC_CHANNEL_THROTTLE 2
#define RC_CHANNEL_YAW 3
#define RC_CHANNEL_ARMED 4
#define RC_CHANNEL_MODE 5

#define MOTOR_REAR_LEFT 0
#define MOTOR_REAR_RIGHT 1
#define MOTOR_FRONT_RIGHT 2
#define MOTOR_FRONT_LEFT 3

float t = NAN; // current step time, s
float dt; // time delta from previous step, s
float loopFreq; // loop frequency, Hz
int16_t channels[16]; // raw rc channels
float controls[RC_CHANNELS]; // normalized controls in range [-1..1] ([0..1] for throttle)
Vector gyro; // gyroscope data
Vector acc; // accelerometer data, m/s/s
Vector rates; // filtered angular rates, rad/s
Quaternion attitude; // estimated attitude
float motors[4]; // normalized motors thrust in range [-1..1]

void setup() {
	Serial.begin(SERIAL_BAUDRATE);
	Serial.println("Initializing flix");
	disableBrownOut();
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
}
