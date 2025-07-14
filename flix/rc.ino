// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Work with the RC receiver

#include <SBUS.h>
#include "util.h"

SBUS RC(Serial2); // NOTE: Use RC(Serial2, 16, 17) if you use the old UART2 pins

uint16_t channels[16]; // raw rc channels
float controlTime; // time of the last controls update


// NOTE: use 'cr' command to calibrate the RC and put the values here
int channelZero[] = {992, 992, 172, 992, 172, 172, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int channelMax[] = {1811, 1811, 1811, 1811, 1811, 1811, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

// Channels mapping:
int rollChannel = 0;
int pitchChannel = 1;
int throttleChannel = 2;
int yawChannel = 3;
int armedChannel = 4;
int modeChannel = 5;

void setupRC() {
	Serial.println("Setup RC");
	RC.begin();
}

bool readRC() {
	if (RC.read()) {
		SBUSData data = RC.data();
		for (int i = 0; i < 16; i++) channels[i] = data.ch[i]; // copy channels data
		normalizeRC();
		controlTime = t;
		return true;
	}
	return false;
}

void normalizeRC() {
	float controls[16];
	for (int i = 0; i < 16; i++) {
		controls[i] = mapf(channels[i], channelZero[i], channelMax[i], 0, 1);
	}
	// Update control values
	controlRoll = controls[rollChannel];
	controlPitch = controls[pitchChannel];
	controlYaw = controls[yawChannel];
	controlThrottle = controls[throttleChannel];
	controlArmed = controls[armedChannel];
	controlMode = controls[modeChannel];
}

void calibrateRC() {
	Serial.println("Calibrate RC: move all sticks to maximum positions [4 sec]");
	Serial.println("··o     ··o\n···     ···\n···     ···");
	delay(4000);
	while (!readRC());
	for (int i = 0; i < 16; i++) {
		channelMax[i] = channels[i];
	}
	Serial.println("Calibrate RC: move all sticks to neutral positions [4 sec]");
	Serial.println("···     ···\n···     ·o·\n·o·     ···");
	delay(4000);
	while (!readRC());
	for (int i = 0; i < 16; i++) {
		channelZero[i] = channels[i];
	}
	printRCCalibration();
}

void printRCCalibration() {
	for (int i = 0; i < sizeof(channelZero) / sizeof(channelZero[0]); i++) Serial.printf("%d ", channelZero[i]);
	Serial.printf("\n");
	for (int i = 0; i < sizeof(channelMax) / sizeof(channelMax[0]); i++) Serial.printf("%d ", channelMax[i]);
	Serial.printf("\n");
}
