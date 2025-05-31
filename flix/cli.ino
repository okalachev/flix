// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Implementation of command line interface

#include "pid.h"
#include "vector.h"

extern const int MOTOR_REAR_LEFT, MOTOR_REAR_RIGHT, MOTOR_FRONT_RIGHT, MOTOR_FRONT_LEFT;
extern float loopRate;

const char* motd =
"\nWelcome to\n"
" _______  __       __  ___   ___\n"
"|   ____||  |     |  | \\  \\ /  /\n"
"|  |__   |  |     |  |  \\  V  /\n"
"|   __|  |  |     |  |   >   <\n"
"|  |     |  `----.|  |  /  .  \\\n"
"|__|     |_______||__| /__/ \\__\\\n\n"
"Commands:\n\n"
"help - show help\n"
"ps - show pitch/roll/yaw\n"
"psq - show attitude quaternion\n"
"imu - show IMU data\n"
"rc - show RC data\n"
"mot - show motor output\n"
"log - dump in-RAM log\n"
"cr - calibrate RC\n"
"cg - calibrate gyro\n"
"ca - calibrate accel\n"
"mfr, mfl, mrr, mrl - test motor (remove props)\n"
"reset - reset drone's state\n";

void doCommand(const String& command) {
	if (command == "help" || command == "motd") {
		Serial.println(motd);
	} else if (command == "ps") {
		Vector a = attitude.toEuler();
		Serial.printf("roll: %f pitch: %f yaw: %f\n", a.x * RAD_TO_DEG, a.y * RAD_TO_DEG, a.z * RAD_TO_DEG);
	} else if (command == "psq") {
		Serial.printf("qx: %f qy: %f qz: %f qw: %f\n", attitude.x, attitude.y, attitude.z, attitude.w);
	} else if (command == "imu") {
		printIMUInfo();
		Serial.printf("gyro: %f %f %f\n", rates.x, rates.y, rates.z);
		Serial.printf("acc: %f %f %f\n", acc.x, acc.y, acc.z);
		printIMUCal();
		Serial.printf("rate: %f\n", loopRate);
	} else if (command == "rc") {
		Serial.printf("Raw: throttle %d yaw %d pitch %d roll %d armed %d mode %d\n",
			channels[RC_CHANNEL_THROTTLE], channels[RC_CHANNEL_YAW], channels[RC_CHANNEL_PITCH],
			channels[RC_CHANNEL_ROLL], channels[RC_CHANNEL_ARMED], channels[RC_CHANNEL_MODE]);
		Serial.printf("Control: throttle %f yaw %f pitch %f roll %f armed %f mode %f\n",
			controls[RC_CHANNEL_THROTTLE], controls[RC_CHANNEL_YAW], controls[RC_CHANNEL_PITCH],
			controls[RC_CHANNEL_ROLL], controls[RC_CHANNEL_ARMED], controls[RC_CHANNEL_MODE]);
		Serial.printf("Mode: %s\n", getModeName());
	} else if (command == "mot") {
		Serial.printf("MOTOR front-right %f front-left %f rear-right %f rear-left %f\n",
			motors[MOTOR_FRONT_RIGHT], motors[MOTOR_FRONT_LEFT], motors[MOTOR_REAR_RIGHT], motors[MOTOR_REAR_LEFT]);
	} else if (command == "log") {
		dumpLog();
	} else if (command == "cr") {
		calibrateRC();
	} else if (command == "cg") {
		calibrateGyro();
	} else if (command == "ca") {
		calibrateAccel();
	} else if (command == "mfr") {
		testMotor(MOTOR_FRONT_RIGHT);
	} else if (command == "mfl") {
		testMotor(MOTOR_FRONT_LEFT);
	} else if (command == "mrr") {
		testMotor(MOTOR_REAR_RIGHT);
	} else if (command == "mrl") {
		testMotor(MOTOR_REAR_LEFT);
	} else if (command == "reset") {
		attitude = Quaternion();
	} else if (command == "") {
		// do nothing
	} else {
		Serial.println("Invalid command: " + command);
	}
}

void handleInput() {
	static bool showMotd = true;
	static String input;

	if (showMotd) {
		Serial.printf("%s\n", motd);
		showMotd = false;
	}

	while (Serial.available()) {
		char c = Serial.read();
		if (c == '\n') {
			doCommand(input);
			input.clear();
		} else {
			input += c;
		}
	}
}
