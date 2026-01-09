// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// All-in-one header file

#pragma once

#include <Arduino.h>
#include "vector.h"
#include "quaternion.h"

extern float t, dt;
extern float loopRate;
extern float controlRoll, controlPitch, controlYaw, controlThrottle, controlMode;
extern Vector gyro, acc;
extern Vector rates;
extern Quaternion attitude;
extern bool landed;
extern int mode;
extern bool armed;
extern Quaternion attitudeTarget;
extern Vector ratesTarget, ratesExtra, torqueTarget;
extern float thrustTarget;
extern float motors[4];

void print(const char* format, ...);
void pause(float duration);
void doCommand(String str, bool echo = false);
void handleInput();
void control();
void interpretControls();
void controlAttitude();
void controlRates();
void controlTorque();
const char *getModeName();
void estimate();
void applyGyro();
void applyAcc();
void setupIMU();
void configureIMU();
void readIMU();
void rotateIMU(Vector& data);
void calibrateGyroOnce();
void calibrateAccel();
void calibrateAccelOnce();
void printIMUCalibration();
void printIMUInfo();
void setupLED();
void setLED(bool on);
void blinkLED();
void prepareLogData();
void logData();
void printLogHeader();
void printLogData();
void processMavlink();
void sendMavlink();
void sendMessage(const void *msg);
void receiveMavlink();
void handleMavlink(const void *_msg);
void mavlinkPrint(const char* str);
void sendMavlinkPrint();
void setupMotors();
int getDutyCycle(float value);
void sendMotors();
bool motorsActive();
void testMotor(int n);
void setupParameters();
int parametersCount();
const char *getParameterName(int index);
float getParameter(int index);
float getParameter(const char *name);
bool setParameter(const char *name, const float value);
void syncParameters();
void printParameters();
void resetParameters();
void setupRC();
bool readRC();
void normalizeRC();
void calibrateRC();
void calibrateRCChannel(float *channel, uint16_t in[16], uint16_t out[16], const char *str);
void printRCCalibration();
void failsafe();
void rcLossFailsafe();
void descend();
void autoFailsafe();
void step();
void computeLoopRate();
void setupWiFi();
void sendWiFi(const uint8_t *buf, int len);
int receiveWiFi(uint8_t *buf, int len);
