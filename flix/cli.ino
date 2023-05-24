// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

#include "pid.h"

static String command;
static String value;
static bool parsingCommand = true;

extern PID rollRatePID, pitchRatePID, yawRatePID, rollPID, pitchPID;

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
"wifi - start wi-fi access point\n";

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
	if (command == "help" || command == "motd") {
		Serial.println(motd);
	} else if (command == "show") {
		showTable();
	} else if (command == "ps") {
		Vector a = attitude.toEulerZYX();
		Serial.printf("roll: %f pitch: %f yaw: %f\n", a.x * RAD_TO_DEG, a.y * RAD_TO_DEG, a.z * RAD_TO_DEG);
	} else if (command == "psq") {
		Serial.printf("qx: %f qy: %f qz: %f qw: %f\n", attitude.x, attitude.y, attitude.z, attitude.w);
	} else if (command == "imu") {
		Serial.printf("gyro: %f %f %f\n", rates.x, rates.y, rates.z);
		Serial.printf("acc: %f %f %f\n", acc.x, acc.y, acc.z);
		printIMUCal();
	} else if (command == "rc") {
		Serial.printf("RAW throttle %d yaw %d pitch %d roll %d aux %d mode %d\n",
			channels[RC_CHANNEL_THROTTLE], channels[RC_CHANNEL_YAW], channels[RC_CHANNEL_PITCH],
			channels[RC_CHANNEL_ROLL], channels[RC_CHANNEL_AUX], channels[RC_CHANNEL_MODE]);
		Serial.printf("CONTROL throttle %f yaw %f pitch %f roll %f aux %f mode %f\n",
			controls[RC_CHANNEL_THROTTLE], controls[RC_CHANNEL_YAW], controls[RC_CHANNEL_PITCH],
			controls[RC_CHANNEL_ROLL], controls[RC_CHANNEL_AUX], controls[RC_CHANNEL_MODE]);
	} else if (command == "mot") {
		Serial.printf("MOTOR front-right %f front-left %f rear-right %f rear-left %f\n",
			motors[MOTOR_FRONT_RIGHT], motors[MOTOR_FRONT_LEFT], motors[MOTOR_REAR_RIGHT], motors[MOTOR_REAR_LEFT]);
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
		// TODO: on error returns 0, check invalid value

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
		Serial.println("Invalid command: " + command);
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
	Serial.printf("Testing motor %d\n", n);
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
