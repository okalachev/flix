// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// All-in-one header file

#pragma once

#include <Arduino.h>
#include "vector.h"
#include "quaternion.h"

// The most used global variables:
extern float t; // current step time, s
extern float dt; // time delta from previous step, s
extern Vector gyro; // gyroscope data
extern Vector acc; // accelerometer data, m/s²
extern Vector rates; // filtered angular rates, rad/s
extern Quaternion attitude; // estimated attitude
extern float controlRoll, controlPitch, controlYaw, controlThrottle, controlMode; // pilot inputs, range [-1, 1]
extern float controlTime; // inputs last update time
extern int mode;
extern bool armed;
extern Vector ratesTarget, ratesExtra, torqueTarget;
extern Quaternion attitudeTarget;
extern float thrustTarget;
extern bool landed; // are we landed and stationary
extern float motors[4]; // normalized motors thrust in range [0..1]

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
void dumpLog();
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
