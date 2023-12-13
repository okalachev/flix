// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Declarations of some functions and variables in Arduino code

#include <cmath>
#include <stdio.h>
#include "vector.h"
#include "quaternion.h"
#include "Arduino.h"

#define RC_CHANNELS 6

#define MOTOR_REAR_LEFT 0
#define MOTOR_FRONT_LEFT 3
#define MOTOR_FRONT_RIGHT 2
#define MOTOR_REAR_RIGHT 1

float t = NAN;
float dt;
float loopFreq;
float motors[4];
int16_t channels[16]; // raw rc channels WARNING: unsigned on hardware
float controls[RC_CHANNELS];
Vector acc;
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
void controlManual();
void controlRate();
void showTable();
bool motorsActive();
void cliTestMotor(uint8_t n);

// mocks
void setLED(bool on) {};
void calibrateGyro() { printf("Skip gyro calibrating\n"); };
void calibrateAccel() { printf("Skip accel calibrating\n"); };
void fullMotorTest(int n, bool reverse) { printf("Skip full motor test\n"); };
void sendMotors() {};
void printIMUCal() { printf("cal: N/A\n"); };
