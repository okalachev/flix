// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Declarations of some functions and variables in Arduino code

#include <cmath>
#include <stdio.h>
#include "vector.h"
#include "quaternion.h"
#include "Arduino.h"
#include "wifi.h"

#define WIFI_ENABLED 1

double t = NAN;
float dt;
float motors[4];
int16_t channels[16]; // raw rc channels
float controls[16];
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
void print(const char* format, ...);
void doCommand(String str, bool echo);
void cliTestMotor(uint8_t n);
void normalizeRC();
void printRCCal();
void processMavlink();
void sendMavlink();
void sendMessage(const void *msg);
void receiveMavlink();
void handleMavlink(const void *_msg);
void mavlinkPrint(const char* str);
void failsafe();
void armingFailsafe();
void rcLossFailsafe();
void descend();
inline Quaternion FLU2FRD(const Quaternion &q);

// mocks
void setLED(bool on) {};
void calibrateGyro() { print("Skip gyro calibrating\n"); };
void calibrateAccel() { print("Skip accel calibrating\n"); };
void printIMUCal() { print("cal: N/A\n"); };
void printIMUInfo() {};
Vector accBias, gyroBias, accScale(1, 1, 1);
