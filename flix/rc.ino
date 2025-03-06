// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Work with the RC receiver

#include <SBUS.h>
#include "util.h"

SBUS RC(Serial2); // NOTE: Use RC(Serial2, 16, 17) if you use the old UART2 pins

// RC channels mapping:
int rollChannel = 0;
int pitchChannel = 1;
int throttleChannel = 2;
int yawChannel = 3;
int armedChannel = 4;
int modeChannel = 5;

double controlsTime; // time of the last controls update
float channelNeutral[16] = {NAN}; // first element NAN means not calibrated
float channelMax[16];

void setupRC() {
	print("Setup RC\n");
	RC.begin();
}

bool readRC() {
	if (RC.read()) {
		SBUSData data = RC.data();
		memcpy(channels, data.ch, sizeof(channels)); // copy channels data
		normalizeRC();
		controlsTime = t;
		return true;
	}
	return false;
}

void normalizeRC() {
	if (isnan(channelNeutral[0])) return; // skip if not calibrated
	for (uint8_t i = 0; i < 16; i++) {
		controls[i] = mapf(channels[i], channelNeutral[i], channelMax[i], 0, 1);
	}
}

void calibrateRC() {
	print("Calibrate RC: move all sticks to maximum positions [4 sec]\n");
	print("··o     ··o\n···     ···\n···     ···\n");
	pause(4);
	while (!readRC());
	for (int i = 0; i < 16; i++) {
		channelMax[i] = channels[i];
	}
	print("Calibrate RC: move all sticks to neutral positions [4 sec]\n");
	print("···     ···\n···     ·o·\n·o·     ···\n");
	pause(4);
	while (!readRC());
	for (int i = 0; i < 16; i++) {
		channelNeutral[i] = channels[i];
	}
	printRCCal();
}

void printRCCal() {
	for (int i = 0; i < sizeof(channelNeutral) / sizeof(channelNeutral[0]); i++) print("%g ", channelNeutral[i]);
	print("\n");
	for (int i = 0; i < sizeof(channelMax) / sizeof(channelMax[0]); i++) print("%g ", channelMax[i]);
	print("\n");
}
