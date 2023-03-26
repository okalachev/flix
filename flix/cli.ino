// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

#include <MPU9250.h>
#include "pid.hpp"

static String command;
static String value;
static bool parsingCommand = true;

extern PID rollRatePID, pitchRatePID, yawRatePID, rollPID, pitchPID;
extern MPU9250 IMU;

const char* motd =
"\nWelcome to\n"
" _______  __       __  ___   ___\n"
"|   ____||  |     |  | \\  \\ /  /\n"
"|  |__   |  |     |  |  \\  V  /\n"
"|   __|  |  |     |  |   >   <\n"
"|  |     |  `----.|  |  /  .  \\\n"
"|__|     |_______||__| /__/ \\__\\\n\n"
"Commands:\n\n"
"show - show all parameters\n"
"<name> <value> - set parameter\n"
"ps - show pitch/roll/yaw\n"
"psq - show attitude quaternion\n"
"imu - show IMU data\n"
"rc - show RC data\n"
"mot - show motor data\n"
"log - dump in-RAM log\n"
"cg - calibrate gyro\n"
"fullmot <n> - test motor on all signals\n"
"wifi - start wi-fi access point\n\n";

bool showMotd = true;

static const struct Param {
	const char* name;
	float* value;
	float* value2;
} params[] = {
	{"rp", &rollRatePID.p, &pitchRatePID.p},
	{"ri", &rollRatePID.i, &pitchRatePID.i},
	{"rd", &rollRatePID.d, &pitchRatePID.d},

	{"ap", &rollPID.p, &pitchPID.p},
	{"ai", &rollPID.i, &pitchPID.i},
	{"ad", &rollPID.d, &pitchPID.d},

	{"yp", &yawRatePID.p, nullptr},
	{"yi", &yawRatePID.i, nullptr},
	{"yd", &yawRatePID.d, nullptr},

	{"ss", &stepsPerSecond, nullptr},
	// {"m", &mode, nullptr},
};

static void doCommand()
{
	if (command == "show") {
		showTable();
	} else if (command == "ps") {
		Vector a = attitude.toEulerZYX();
		Serial.println("roll: " + String(a.x * RAD_TO_DEG, 2) +
			" pitch: " + String(a.y * RAD_TO_DEG, 2) +
			" yaw: " + String(a.z * RAD_TO_DEG, 2));
	} else if (command == "psq") {
		Serial.println("qx: " + String(attitude.x) +
			" qy: " + String(attitude.y) +
			" qz: " + String(attitude.z) +
			" qw: " + String(attitude.w));
	} else if (command == "imu") {
		Serial.println("gyro bias " + String(IMU.getGyroBiasX_rads()) + " "
			+ String(IMU.getGyroBiasY_rads()) + " "
			+ String(IMU.getGyroBiasZ_rads()));
	} else if (command == "rc") {
		Serial.println("RAW throttle " + String(channels[RC_CHANNEL_THROTTLE]) +
			" yaw " + String(channels[RC_CHANNEL_YAW]) +
			" pitch " + String(channels[RC_CHANNEL_PITCH]) +
			" roll " + String(channels[RC_CHANNEL_ROLL]) +
			" aux " + String(channels[RC_CHANNEL_AUX]) +
			" mode " + String(channels[RC_CHANNEL_MODE]));
		Serial.println("CONTROL throttle " + String(controls[RC_CHANNEL_THROTTLE]) +
			" yaw " + String(controls[RC_CHANNEL_YAW]) +
			" pitch " + String(controls[RC_CHANNEL_PITCH]) +
			" roll " + String(controls[RC_CHANNEL_ROLL]) +
			" aux " + String(controls[RC_CHANNEL_AUX]) +
			" mode " + String(controls[RC_CHANNEL_MODE]));
	} else if (command == "mot") {
		Serial.println("MOTOR front-right " + String(motors[MOTOR_FRONT_RIGHT]) +
			" front-left " + String(motors[MOTOR_FRONT_LEFT]) +
			" rear-right " + String(motors[MOTOR_REAR_RIGHT]) +
			" rear-left " + String(motors[MOTOR_REAR_LEFT]));
	} else if (command == "log") {
		dumpLog();
	} else if (command == "cg") {
		calibrateGyro();
	} else if (command == "mfr") {
		cliTestMotor(MOTOR_FRONT_RIGHT);
	} else if (command == "mfl") {
		cliTestMotor(MOTOR_FRONT_LEFT);
	} else if (command == "mrr") {
		cliTestMotor(MOTOR_REAR_RIGHT);
	} else if (command == "mrl") {
		cliTestMotor(MOTOR_REAR_LEFT);
	} else if (command == "fullmot") {
		fullMotorTest(value.toInt());
	} else {
		float val = value.toFloat();
		if (!isfinite(val)) {
			Serial.println("Invalid value: " + value);
		}

		for (uint8_t i = 0; i < sizeof(params) / sizeof(params[0]); i++) {
			if (command == params[i].name) {
				*params[i].value = val;
				if (params[i].value2 != nullptr) *params[i].value2 = val;
				Serial.print(command);
				Serial.print(" = ");
				Serial.println(val, 4);
				return;
			}
		}
		Serial.println("Invalid command: '" + command + "'");
	}
}

static void showTable()
{
	for (uint8_t i = 0; i < sizeof(params) / sizeof(params[0]); i++) {
		Serial.print(params[i].name);
		Serial.print(" ");
		Serial.println(*params[i].value, 5);
	}
}

static void cliTestMotor(uint8_t n)
{
	Serial.println("Testing motor " + String(n));
	motors[n] = 1;
	sendMotors();
	delay(5000);
	motors[n] = 0;
	sendMotors();
	Serial.println("Done");
}

void parseInput()
{
	if (showMotd) {
		Serial.println(motd);
		showMotd = false;
	}

	while (Serial.available()) {
		char c = Serial.read();
		if (c == '\n') {
			parsingCommand = true;
			if (!command.isEmpty()) {
				doCommand();
			}
			command.clear();
			value.clear();
		} else if (c == ' ') {
			parsingCommand = false;
		} else {
			(parsingCommand ? command : value) += c;
		}
	}
}
