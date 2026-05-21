// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Main firmware file

#include "config.h"
#include "vector.h"
#include "quaternion.h"
#include "util.h"
#include "flix.h"

void setup() {
	Serial.begin(115200);
	print("Initializing flix\n");
	setupParameters();
	setupPower();
	setupLED();
	setLED(true);
	setupMotors();
	setupWiFi();
	setupIMU();
	setupRC();
	setLED(false);
	print("Initializing complete\n");
}

void loop() {
	readIMU();
	step();
	readRC();
	estimate();
	control();
	sendMotors();
	handleInput();
	processMavlink();
	readVoltage();
	logData();
	syncParameters();
}
