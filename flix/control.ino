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
#define PITCHRATE_MAX radians(360)
#define ROLLRATE_MAX radians(360)
#define YAWRATE_MAX radians(360)
#define MAX_TILT radians(30)

#define RATES_D_LPF_ALPHA 0.2 // cutoff frequency ~ 40 Hz

enum { MANUAL, ACRO, STAB, USER } mode = STAB;
enum { YAW, YAW_RATE } yawMode = YAW;
bool armed = false;

PID rollRatePID(ROLLRATE_P, ROLLRATE_I, ROLLRATE_D, ROLLRATE_I_LIM, RATES_D_LPF_ALPHA);
PID pitchRatePID(PITCHRATE_P, PITCHRATE_I, PITCHRATE_D, PITCHRATE_I_LIM, RATES_D_LPF_ALPHA);
PID yawRatePID(YAWRATE_P, YAWRATE_I, YAWRATE_D);
PID rollPID(ROLL_P, ROLL_I, ROLL_D);
PID pitchPID(PITCH_P, PITCH_I, PITCH_D);
PID yawPID(YAW_P, 0, 0);

Quaternion attitudeTarget;
Vector ratesTarget;
Vector torqueTarget;
float thrustTarget;

void control() {
	interpretRC();
	if (mode == STAB) {
		controlAttitude();
		controlRate();
		controlTorque();
	} else if (mode == ACRO) {
		controlRate();
		controlTorque();
	} else if (mode == MANUAL) {
		controlTorque();
	}
}

void interpretRC() {
	armed = controls[RC_CHANNEL_THROTTLE] >= 0.05 && controls[RC_CHANNEL_ARMED] >= 0.5;

	// NOTE: put ACRO or MANUAL modes there if you want to use them
	if (controls[RC_CHANNEL_MODE] < 0.25) {
		mode = STAB;
	} else if (controls[RC_CHANNEL_MODE] < 0.75) {
		mode = STAB;
	} else {
		mode = STAB;
	}

	thrustTarget = controls[RC_CHANNEL_THROTTLE];

	if (mode == ACRO) {
		yawMode = YAW_RATE;
		ratesTarget.x = controls[RC_CHANNEL_ROLL] * ROLLRATE_MAX;
		ratesTarget.y = -controls[RC_CHANNEL_PITCH] * PITCHRATE_MAX; // up pitch stick means tilt clockwise in frd
		ratesTarget.z = controls[RC_CHANNEL_YAW] * YAWRATE_MAX;

	} else if (mode == STAB) {
		yawMode = controls[RC_CHANNEL_YAW] == 0 ? YAW : YAW_RATE;

		attitudeTarget = Quaternion::fromEulerZYX(Vector(
			controls[RC_CHANNEL_ROLL] * MAX_TILT,
			-controls[RC_CHANNEL_PITCH] * MAX_TILT,
			attitudeTarget.getYaw()));
		ratesTarget.z = controls[RC_CHANNEL_YAW] * YAWRATE_MAX;

	} else if (mode == MANUAL) {
		// passthrough mode
		yawMode = YAW_RATE;
		torqueTarget = Vector(controls[RC_CHANNEL_ROLL], -controls[RC_CHANNEL_PITCH], controls[RC_CHANNEL_YAW]) * 0.01;
	}

	if (yawMode == YAW_RATE || !motorsActive()) {
		// update yaw target as we don't have control over the yaw
		attitudeTarget.setYaw(attitude.getYaw());
	}
}

void controlAttitude() {
	if (!armed) {
		rollPID.reset();
		pitchPID.reset();
		yawPID.reset();
		return;
	}

	const Vector up(0, 0, -1);
	Vector upActual = attitude.rotate(up);
	Vector upTarget = attitudeTarget.rotate(up);

	Vector error = Vector::angularRatesBetweenVectors(upTarget, upActual);

	ratesTarget.x = rollPID.update(error.x, dt);
	ratesTarget.y = pitchPID.update(error.y, dt);

	if (yawMode == YAW) {
		ratesTarget.z = yawPID.update(wrapAngle(attitudeTarget.getYaw() - attitude.getYaw()), dt);
	}
}

void controlRate() {
	if (!armed) {
		rollRatePID.reset();
		pitchRatePID.reset();
		yawRatePID.reset();
		return;
	}

	Vector error = ratesTarget - rates;

	// Calculate desired torque, where 0 - no torque, 1 - maximum possible torque
	torqueTarget.x = rollRatePID.update(error.x, dt);
	torqueTarget.y = pitchRatePID.update(error.y, dt);
	torqueTarget.z = yawRatePID.update(error.z, dt);
}

void controlTorque() {
	if (!armed) {
		memset(motors, 0, sizeof(motors));
		return;
	}

	motors[MOTOR_FRONT_LEFT] = thrustTarget + torqueTarget.x + torqueTarget.y - torqueTarget.z;
	motors[MOTOR_FRONT_RIGHT] = thrustTarget - torqueTarget.x + torqueTarget.y + torqueTarget.z;
	motors[MOTOR_REAR_LEFT] = thrustTarget + torqueTarget.x - torqueTarget.y + torqueTarget.z;
	motors[MOTOR_REAR_RIGHT] = thrustTarget - torqueTarget.x - torqueTarget.y - torqueTarget.z;

	motors[0] = constrain(motors[0], 0, 1);
	motors[1] = constrain(motors[1], 0, 1);
	motors[2] = constrain(motors[2], 0, 1);
	motors[3] = constrain(motors[3], 0, 1);
}

bool motorsActive() {
	return motors[0] > 0 || motors[1] > 0 || motors[2] > 0 || motors[3] > 0;
}

const char* getModeName() {
	switch (mode) {
		case MANUAL: return "MANUAL";
		case ACRO: return "ACRO";
		case STAB: return "STAB";
		case USER: return "USER";
		default: return "UNKNOWN";
	}
}
