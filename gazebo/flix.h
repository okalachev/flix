// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Declarations of some functions and variables in Arduino code

#include <cmath>
#include <stdio.h>
#include "vector.h"
#include "quaternion.h"
#include "Arduino.h"
#include "wifi.h"

#define RC_CHANNELS 16
#define WIFI_ENABLED 1

float t = NAN;
float dt;
float motors[4];
int16_t channels[16]; // raw rc channels
float controls[RC_CHANNELS];
float controlsTime;
Vector acc;
Vector gyro;
Vector rates;
Quaternion attitude;

// declarations
void computeLoopRate();
void applyGyro();
void applyAcc();
void control();
void interpretRC();
void controlAttitude();
void controlRate();
void controlTorque();
void showTable();
void sendMotors();
bool motorsActive();
void cliTestMotor(uint8_t n);
void printRCCal();
void processMavlink();
void sendMavlink();
void sendMessage(const void *msg);
void receiveMavlink();
void handleMavlink(const void *_msg);
void failsafe();
void descend();
inline Quaternion FLU2FRD(const Quaternion &q);

// mocks
void setLED(bool on) {};
void calibrateGyro() { printf("Skip gyro calibrating\n"); };
void calibrateAccel() { printf("Skip accel calibrating\n"); };
void printIMUCal() { printf("cal: N/A\n"); };
void printIMUInfo() {};
