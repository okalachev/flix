// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Work with the RC receiver

#include <SBUS.h>
#include "util.h"

// RC channels mapping:
int rollChannel = 0;
int pitchChannel = 1;
int throttleChannel = 2;
int yawChannel = 3;
int armedChannel = 4;
int modeChannel = 5;

float controlsTime; // time of the last controls update
float channelNeutral[16] = {NAN}; // first element NAN means not calibrated
float channelMax[16];

SBUS RC(Serial2); // NOTE: Use RC(Serial2, 16, 17) if you use the old UART2 pins

void setupRC() {
	Serial.println("Setup RC");
	RC.begin();
}

void readRC() {
	if (RC.read()) {
		SBUSData data = RC.data();
		memcpy(channels, data.ch, sizeof(channels)); // copy channels data
		normalizeRC();
		controlsTime = t;
	}
}

void normalizeRC() {
	if (isnan(channelNeutral[0])) return; // skip if not calibrated
	for (uint8_t i = 0; i < 16; i++) {
		controls[i] = mapf(channels[i], channelNeutral[i], channelMax[i], 0, 1);
	}
}

void calibrateRC() {
	Serial.println("Calibrate RC: move all sticks to maximum positions in 4 seconds");
	Serial.println("··o     ··o\n···     ···\n···     ···");
	delay(4000);
	for (int i = 0; i < 30; i++) readRC(); // ensure the values are updated
	for (int i = 0; i < 16; i++) {
		channelMax[i] = channels[i];
	}
	Serial.println("Calibrate RC: move all sticks to neutral positions in 4 seconds");
	Serial.println("···     ···\n···     ·o·\n·o·     ···");
	delay(4000);
	for (int i = 0; i < 30; i++) readRC(); // ensure the values are updated
	for (int i = 0; i < 16; i++) {
		channelNeutral[i] = channels[i];
	}
	printRCCal();
}

void printRCCal() {
	printArray(channelNeutral);
	printArray(channelMax);
}
