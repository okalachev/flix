// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Implementation of command line interface

#include "pid.h"
#include "vector.h"

extern const int MOTOR_REAR_LEFT, MOTOR_REAR_RIGHT, MOTOR_FRONT_RIGHT, MOTOR_FRONT_LEFT;
extern float loopRate, dt;
extern double t;
extern int rollChannel, pitchChannel, throttleChannel, yawChannel, armedChannel, modeChannel;

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
"p - show all parameters\n"
"p <name> - show parameter\n"
"p <name> <value> - set parameter\n"
"preset - reset parameters\n"
"time - show time info\n"
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
"reset - reset drone's state\n"
"reboot - reboot the drone\n";

void doCommand(String str) {
	// parse command
	String command, arg0, arg1;
	splitString(str, command, arg0, arg1);

	// execute command
	if (command == "help" || command == "motd") {
		Serial.println(motd);
	} else if (command == "p" && arg0 == "") {
		printParameters();
	} else if (command == "p" && arg0 != "" && arg1 == "") {
		Serial.printf("%s = %g\n", arg0.c_str(), getParameter(arg0.c_str()));
	} else if (command == "p") {
		bool success = setParameter(arg0.c_str(), arg1.toFloat());
		if (success) {
			Serial.printf("%s = %g\n", arg0.c_str(), arg1.toFloat());
		} else {
			Serial.printf("Parameter not found: %s\n", arg0.c_str());
		}
	} else if (command == "preset") {
		resetParameters();
	} else if (command == "time") {
		Serial.printf("Time: %f\n", t);
		Serial.printf("Loop rate: %f\n", loopRate);
		Serial.printf("dt: %f\n", dt);
	} else if (command == "ps") {
		Vector a = attitude.toEulerZYX();
		Serial.printf("roll: %f pitch: %f yaw: %f\n", degrees(a.x), degrees(a.y), degrees(a.z));
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
			channels[throttleChannel], channels[yawChannel], channels[pitchChannel],
			channels[rollChannel], channels[armedChannel], channels[modeChannel]);
		Serial.printf("Control: throttle %g yaw %g pitch %g roll %g armed %g mode %g\n",
			controls[throttleChannel], controls[yawChannel], controls[pitchChannel],
			controls[rollChannel], controls[armedChannel], controls[modeChannel]);
		Serial.printf("Mode: %s\n", getModeName());
	} else if (command == "mot") {
		Serial.printf("Motors: front-right %g front-left %g rear-right %g rear-left %g\n",
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
	} else if (command == "reboot") {
		ESP.restart();
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
		Serial.println(motd);
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
