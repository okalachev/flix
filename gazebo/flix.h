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
void step();
void computeLoopRate();
void applyGyro();
void applyAcc();
void control();
void interpretRC();
void controlAttitude();
void controlRate();
void controlTorque();
const char* getModeName();
void sendMotors();
bool motorsActive();
void testMotor(uint8_t n);
void print(const char* format, ...);
void pause(float duration);
void doCommand(String str, bool echo);
void handleInput();
void calibrateRC();
void normalizeRC();
void printRCCal();
void dumpLog();
void processMavlink();
void sendMavlink();
void sendMessage(const void *msg);
void receiveMavlink();
void handleMavlink(const void *_msg);
void mavlinkPrint(const char* str);
inline Quaternion fluToFrd(const Quaternion &q);
void failsafe();
void armingFailsafe();
void rcLossFailsafe();
void descend();
int parametersCount();
const char *getParameterName(int index);
float getParameter(int index);
float getParameter(const char *name);
bool setParameter(const char *name, const float value);
void printParameters();
void resetParameters();

// mocks
void setLED(bool on) {};
void calibrateGyro() { print("Skip gyro calibrating\n"); };
void calibrateAccel() { print("Skip accel calibrating\n"); };
void printIMUCal() { print("cal: N/A\n"); };
void printIMUInfo() {};
Vector accBias, gyroBias, accScale(1, 1, 1);
