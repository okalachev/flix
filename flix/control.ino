// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Flight control

#include "vector.h"
#include "quaternion.h"
#include "pid.h"
#include "lpf.h"

#define PITCHRATE_P 0.05
#define PITCHRATE_I 0.2
#define PITCHRATE_D 0.001
#define PITCHRATE_I_LIM 0.3
#define ROLLRATE_P PITCHRATE_P
#define ROLLRATE_I PITCHRATE_I
#define ROLLRATE_D PITCHRATE_D
#define ROLLRATE_I_LIM PITCHRATE_I_LIM
#define YAWRATE_P 0.3
#define YAWRATE_I 0.0
#define YAWRATE_D 0.0
#define YAWRATE_I_LIM 0.3
#define ROLL_P 4.5
#define ROLL_I 0
#define ROLL_D 0
#define PITCH_P ROLL_P
#define PITCH_I ROLL_I
#define PITCH_D ROLL_D
#define YAW_P 3
#define PITCHRATE_MAX 360 * DEG_TO_RAD
#define ROLLRATE_MAX 360 * DEG_TO_RAD
#define YAWRATE_MAX 360 * DEG_TO_RAD
#define MAX_TILT 30 * DEG_TO_RAD

#define RATES_LFP_ALPHA 0.8 // cutoff frequency ~ 250 Hz
#define RATES_D_LPF_ALPHA 0.2 // cutoff frequency ~ 40 Hz

enum { MANUAL, ACRO, STAB } mode = STAB;
enum { YAW, YAW_RATE } yawMode = YAW;
bool armed = false;

PID rollRatePID(ROLLRATE_P, ROLLRATE_I, ROLLRATE_D, ROLLRATE_I_LIM, RATES_D_LPF_ALPHA);
PID pitchRatePID(PITCHRATE_P, PITCHRATE_I, PITCHRATE_D, PITCHRATE_I_LIM, RATES_D_LPF_ALPHA);
PID yawRatePID(YAWRATE_P, YAWRATE_I, YAWRATE_D);
PID rollPID(ROLL_P, ROLL_I, ROLL_D);
PID pitchPID(PITCH_P, PITCH_I, PITCH_D);
PID yawPID(YAW_P, 0, 0);

LowPassFilter<Vector> ratesFilter(RATES_LFP_ALPHA);

Quaternion attitudeTarget;
Vector ratesTarget;
Vector torqueTarget;
float thrustTarget;

void control()
{
	interpretRC();
	if (mode == STAB) {
		controlAttitude();
		controlRate();
	} else if (mode == ACRO) {
		controlRate();
	} else if (mode == MANUAL) {
		controlManual();
	}
}

void interpretRC()
{
	if (controls[RC_CHANNEL_MODE] < 0.25) {
		mode = MANUAL;
	} else if (controls[RC_CHANNEL_MODE] < 0.75) {
		mode = ACRO;
	} else {
		mode = STAB;
	}

	armed = controls[RC_CHANNEL_THROTTLE] >= 0.05 && controls[RC_CHANNEL_AUX] >= 0.5;

	thrustTarget = controls[RC_CHANNEL_THROTTLE];

	if (mode == ACRO) {
		yawMode = YAW_RATE;
		ratesTarget.x = controls[RC_CHANNEL_ROLL] * ROLLRATE_MAX;
		ratesTarget.y = -controls[RC_CHANNEL_PITCH] * PITCHRATE_MAX; // up pitch stick means tilt clockwise in frd
		ratesTarget.z = controls[RC_CHANNEL_YAW] * YAWRATE_MAX;

	} else if (mode == STAB) {
		yawMode = controls[RC_CHANNEL_YAW] == 0 ? YAW : YAW_RATE;

		attitudeTarget = Quaternion::fromEulerZYX(
			controls[RC_CHANNEL_ROLL] * MAX_TILT,
			-controls[RC_CHANNEL_PITCH] * MAX_TILT,
			attitudeTarget.getYaw());
		ratesTarget.z = controls[RC_CHANNEL_YAW] * YAWRATE_MAX;
	}

	if (yawMode == YAW_RATE || !motorsActive()) {
		// update yaw target as we have not control over the yaw
		attitudeTarget.setYaw(attitude.getYaw());
	}
}

void controlAttitude()
{
	if (!armed) {
		rollPID.reset();
		pitchPID.reset();
		yawPID.reset();
		return;
	}

	const Vector up(0, 0, -1);
	Vector upActual = attitude.rotate(up);
	Vector upTarget = attitudeTarget.rotate(up);

	float angle = Vector::angleBetweenVectors(upTarget, upActual);
	Vector ratesTargetDir = Vector::angularRatesBetweenVectors(upTarget, upActual);
	ratesTargetDir.normalize();

	ratesTarget.x = rollPID.update(ratesTargetDir.x * angle, dt);
	ratesTarget.y = pitchPID.update(ratesTargetDir.y * angle, dt);

	if (yawMode == YAW) {
		ratesTarget.z = yawPID.update(wrapAngle(attitudeTarget.getYaw() - attitude.getYaw()), dt);
	}
}

void controlRate()
{
	if (!armed) {
		memset(motors, 0, sizeof(motors));
		rollRatePID.reset();
		pitchRatePID.reset();
		yawRatePID.reset();
		return;
	}

	Vector ratesFiltered = ratesFilter.update(rates);

	torqueTarget.x = rollRatePID.update(ratesTarget.x - ratesFiltered.x, dt); // un-normalized "torque"
	torqueTarget.y = pitchRatePID.update(ratesTarget.y - ratesFiltered.y, dt);
	torqueTarget.z = yawRatePID.update(ratesTarget.z - ratesFiltered.z, dt);

	motors[MOTOR_FRONT_LEFT] = thrustTarget + torqueTarget.y + torqueTarget.x - torqueTarget.z;
	motors[MOTOR_FRONT_RIGHT] = thrustTarget + torqueTarget.y - torqueTarget.x + torqueTarget.z;
	motors[MOTOR_REAR_LEFT] = thrustTarget - torqueTarget.y + torqueTarget.x + torqueTarget.z;
	motors[MOTOR_REAR_RIGHT] = thrustTarget - torqueTarget.y - torqueTarget.x - torqueTarget.z;

	motors[0] = constrain(motors[0], 0, 1);
	motors[1] = constrain(motors[1], 0, 1);
	motors[2] = constrain(motors[2], 0, 1);
	motors[3] = constrain(motors[3], 0, 1);
}

bool motorsActive()
{
	return motors[0] > 0 || motors[1] > 0 || motors[2] > 0 || motors[3] > 0;
}

const char* getModeName()
{
	switch (mode) {
		case MANUAL: return "MANUAL";
		case ACRO: return "ACRO";
		case STAB: return "STAB";
		default: return "UNKNOWN";
	}
}
