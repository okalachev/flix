// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

#include <SBUS.h>

static const uint16_t channelNeutral[] = {995, 883, 200, 972, 512, 512};
static const uint16_t channelMax[] = {1651, 1540, 1713, 1630, 1472, 1472};

static SBUS RC(Serial2);

void setupRC()
{
	Serial.println("Setup RC");
	RC.begin();
}

void readRC()
{
	bool failSafe, lostFrame;
	if (RC.read(channels, &failSafe, &lostFrame)) {
		if (failSafe) { rcFailSafe++; return; } // TODO: NOT TESTED YET
		if (lostFrame) { rcLostFrame++; return; }
		normalizeRC();
	}
}

static void normalizeRC() {
	for (uint8_t i = 0; i < RC_CHANNELS; i++) {
		controls[i] = mapf(channels[i], channelNeutral[i], channelMax[i], 0, 1);
	}
}
