// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

#include "vector.hpp"
#include "quaternion.hpp"

#define SERIAL_BAUDRATE 115200

#define WIFI_ENABLED 0

#define RC_CHANNELS 6
#define RC_CHANNEL_THROTTLE 2
#define RC_CHANNEL_YAW 3
#define RC_CHANNEL_PITCH 1
#define RC_CHANNEL_ROLL 0
#define RC_CHANNEL_AUX 4
#define RC_CHANNEL_MODE 5

#define MOTOR_REAR_LEFT 0
#define MOTOR_FRONT_LEFT 3
#define MOTOR_FRONT_RIGHT 2
#define MOTOR_REAR_RIGHT 1

uint32_t startTime; // system startup time
uint32_t stepTime; // current step time
uint32_t steps; // total steps count
float stepsPerSecond; // steps per last second
float dt; // time delta from previous step
uint16_t channels[16]; // raw rc channels
float controls[RC_CHANNELS]; // normalized controls in range [-1..1] ([0..1] for throttle)
uint32_t rcFailSafe, rcLostFrame;
float motors[4]; // normalized motors thrust in range [-1..1]
Vector rates; // angular rates, rad/s
Vector acc; // accelerometer data, m/s/s
Quaternion attitude; // estimated attitude
bool calibrating; // flag we're calibrating

void setupDebug();
void lowPowerMode();

void setup()
{
	Serial.begin(SERIAL_BAUDRATE);
	Serial.println("Initializing flix");
	setupTime();
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

void loop()
{
	if (!readIMU()) return;

	step();
	readRC();
	estimate();
	control();
	sendMotors();
	parseInput();
#if WIFI_ENABLED == 1
	sendMavlink();
#endif
	logData();
	signalizeHorizontality();
}
