// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// PWM control for motors

#include "util.h"

float motors[4]; // normalized motor thrusts in range [0..1]

int motorPins[4] = {12, 13, 14, 15}; // default pin numbers
int pwmFrequency = 78000;
int pwmResolution = 10;
int pwmStop = 0;
int pwmMin = 0;
int pwmMax = -1; // -1 means duty cycle mode

const int MOTOR_REAR_LEFT = 0;
const int MOTOR_REAR_RIGHT = 1;
const int MOTOR_FRONT_RIGHT = 2;
const int MOTOR_FRONT_LEFT = 3;

void setupMotors() {
	print("Setup Motors\n");
	// configure pins
	for (int i = 0; i < 4; i++) {
		ledcAttach(motorPins[i], pwmFrequency, pwmResolution);
		pwmFrequency = ledcChangeFrequency(motorPins[i], pwmFrequency, pwmResolution); // if re-initializing
	}
	sendMotors();
	print("Motors initialized\n");
}

void sendMotors() {
	for (int i = 0; i < 4; i++) {
		ledcWrite(motorPins[i], getDutyCycle(motors[i]));
	}
}

int getDutyCycle(float value) {
	value = constrain(value, 0, 1);
	if (pwmMax >= 0) { // pwm mode
		float pwm = mapf(value, 0, 1, pwmMin, pwmMax);
		if (value == 0) pwm = pwmStop;
		float duty = mapf(pwm, 0, 1000000 / pwmFrequency, 0, (1 << pwmResolution) - 1);
		return round(duty);
	} else { // duty cycle mode
		return round(value * ((1 << pwmResolution) - 1));
	}
}

bool motorsActive() {
	return motors[0] != 0 || motors[1] != 0 || motors[2] != 0 || motors[3] != 0;
}

void testMotor(int n) {
	print("Testing motor %d\n", n);
	motors[n] = 1;
	delay(50); // ESP32 may need to wait until the end of the current cycle to change duty https://github.com/espressif/arduino-esp32/issues/5306
	sendMotors();
	pause(3);
	motors[n] = 0;
	sendMotors();
	print("Done\n");
}
