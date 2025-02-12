// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Work with the RC receiver

#include <SBUS.h>
#include "util.h"

SBUS RC(Serial2); // NOTE: Use RC(Serial2, 16, 17) if you use the old UART2 pins

// NOTE: use 'cr' command to calibrate the RC and put the values here
int channelNeutral[] = {995, 883, 200, 972, 512, 512, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int channelMax[] = {1651, 1540, 1713, 1630, 1472, 1472, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
float controlsTime; // time of the last controls update

void setupRC() {
	Serial.println("Setup RC");
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
	for (uint8_t i = 0; i < 16; i++) {
		controls[i] = mapf(channels[i], channelNeutral[i], channelMax[i], 0, 1);
	}
}

void calibrateRC() {
	Serial.println("Calibrate RC: move all sticks to maximum positions in 4 seconds");
	Serial.println("··o     ··o\n···     ···\n···     ···");
	delay(4000);
	while (!readRC());
	for (int i = 0; i < 16; i++) {
		channelMax[i] = channels[i];
	}
	Serial.println("Calibrate RC: move all sticks to neutral positions in 4 seconds");
	Serial.println("···     ···\n···     ·o·\n·o·     ···");
	delay(4000);
	while (!readRC());
	for (int i = 0; i < 16; i++) {
		channelNeutral[i] = channels[i];
	}
	printRCCal();
}

void printRCCal() {
	printArray(channelNeutral, 16);
	printArray(channelMax, 16);
}
