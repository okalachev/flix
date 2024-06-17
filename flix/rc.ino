// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Work with the RC receiver

#include <SBUS.h>

// NOTE: use 'cr' command to calibrate the RC and put the values here
int channelNeutral[] = {995, 883, 200, 972, 512, 512};
int channelMax[] = {1651, 1540, 1713, 1630, 1472, 1472};

SBUS RC(Serial2, 16, 17); // NOTE: remove pin numbers (16, 17) if you use the new default pins for Serial2 (4, 25)

void setupRC() {
	Serial.println("Setup RC");
	RC.begin();
}

void readRC() {
	if (RC.read()) {
		SBUSData data = RC.data();
		memcpy(channels, data.ch, sizeof(channels)); // copy channels data
		normalizeRC();
	}
}

void normalizeRC() {
	for (uint8_t i = 0; i < RC_CHANNELS; i++) {
		controls[i] = mapf(channels[i], channelNeutral[i], channelMax[i], 0, 1);
	}
}

void calibrateRC() {
	Serial.println("Calibrate RC: move all sticks to maximum positions within 4 seconds");
	Serial.println("··o     ··o\n···     ···\n···     ···");
	delay(4000);
	for (int i = 0; i < 30; i++) readRC(); // ensure the values are updated
	for (int i = 0; i < RC_CHANNELS; i++) {
		channelMax[i] = channels[i];
	}
	Serial.println("Calibrate RC: move all sticks to neutral positions within 4 seconds");
	Serial.println("···     ···\n···     ·o·\n·o·     ···");
	delay(4000);
	for (int i = 0; i < 30; i++) readRC(); // ensure the values are updated
	for (int i = 0; i < RC_CHANNELS; i++) {
		channelNeutral[i] = channels[i];
	}
	printRCCal();
}

void printRCCal() {
	printArray(channelNeutral, RC_CHANNELS);
	printArray(channelMax, RC_CHANNELS);
}
