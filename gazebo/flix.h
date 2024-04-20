// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Declarations of some functions and variables in Arduino code

#include <cmath>
#include <stdio.h>
#include "vector.h"
#include "quaternion.h"
#include "Arduino.h"
#include "wifi.h"

#define RC_CHANNELS 6

#define MOTOR_REAR_LEFT 0
#define MOTOR_FRONT_LEFT 3
#define MOTOR_FRONT_RIGHT 2
#define MOTOR_REAR_RIGHT 1

#define WIFI_ENABLED 1

float t = NAN;
float dt;
float loopFreq;
float motors[4];
int16_t channels[16]; // raw rc channels
float controls[RC_CHANNELS];
Vector acc;
Vector gyro;
Vector rates;
Quaternion attitude;

// declarations
void computeLoopFreq();
void applyGyro();
void applyAcc();
void signalizeHorizontality();
void control();
void interpretRC();
void controlAttitude();
void controlRate();
void controlTorque();
void showTable();
bool motorsActive();
void cliTestMotor(uint8_t n);
void printRCCal();
void processMavlink();
void sendMavlink();
void sendMessage(const void *msg);
void receiveMavlink();
void handleMavlink(const void *_msg);

// mocks
void setLED(bool on) {};
void calibrateGyro() { printf("Skip gyro calibrating\n"); };
void calibrateAccel() { printf("Skip accel calibrating\n"); };
void fullMotorTest(int n, bool reverse) { printf("Skip full motor test\n"); };
void sendMotors() {};
void printIMUCal() { printf("cal: N/A\n"); };
