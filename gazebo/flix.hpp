// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

#include <cmath>
#include "vector.hpp"
#include "quaternion.hpp"

#define ONE_G 9.807f

#define RC_CHANNELS 6
// #define RC_CHANNEL_THROTTLE 2
// #define RC_CHANNEL_YAW 3
// #define RC_CHANNEL_PITCH 1
// #define RC_CHANNEL_ROLL 0
// #define RC_CHANNEL_AUX 4
// #define RC_CHANNEL_MODE 5

#define MOTOR_REAR_LEFT 0
#define MOTOR_FRONT_LEFT 3
#define MOTOR_FRONT_RIGHT 2
#define MOTOR_REAR_RIGHT 1

Vector acc;
Vector rates;
Quaternion attitude;
float dt = NAN;
float motors[4]; // normalized motors thrust in range [-1..1]
int16_t channels[16]; // raw rc channels WARNING: unsigned in real life
float controls[RC_CHANNELS]; // normalized controls in range [-1..1] ([0..1] for thrust)
uint32_t stepTime;

// util
float mapf(long x, long in_min, long in_max, float out_min, float out_max);
float mapff(float x, float in_min, float in_max, float out_min, float out_max);
// float hypot3(float x, float y, float z);

// rc
void normalizeRC();

// control
void control();
void interpretRC();
static void controlAttitude();
static void controlAttitudeAlter();
static void controlManual();
static void controlRate();
void desaturate(float& a, float& b, float& c, float& d);
static void indicateSaturation();

// mocks
void setLED(bool on) {}
