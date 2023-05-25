// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Flight control

#include "pid.h"
#include "vector.h"
#include "quaternion.h"

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
#define PITCH_P ROLL_P // 8
#define PITCH_I ROLL_I
#define PITCH_D ROLL_D
#define YAW_P 3
#define PITCHRATE_MAX 360 * DEG_TO_RAD
#define ROLLRATE_MAX 360 * DEG_TO_RAD
#define YAWRATE_MAX 360 * DEG_TO_RAD
#define MAX_TILT 30 * DEG_TO_RAD

enum { MANUAL, ACRO, STAB } mode = STAB;
bool armed = false;

PID rollRatePID(ROLLRATE_P, ROLLRATE_I, ROLLRATE_D, ROLLRATE_I_LIM);
PID pitchRatePID(PITCHRATE_P, PITCHRATE_I, PITCHRATE_D, PITCHRATE_I_LIM);
PID yawRatePID(YAWRATE_P, YAWRATE_I, YAWRATE_D);
PID rollPID(ROLL_P, ROLL_I, ROLL_D);
PID pitchPID(PITCH_P, PITCH_I, PITCH_D);
PID yawPID(YAW_P, 0, 0);

Vector ratesFiltered;

Quaternion attitudeTarget;
Vector ratesTarget;
Vector torqueTarget;
float thrustTarget;
 // TODO: ugly
float yawTarget = NAN;
bool controlYaw = false;

void control()
{
	interpretRC();
	if (mode == STAB) {
		controlAttitude();
		// controlAttitudeAlter();
	}

	if (mode == MANUAL) {
		controlManual();
	} else {
		controlRate();
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

	armed = controls[RC_CHANNEL_THROTTLE] >= 0.1 && controls[RC_CHANNEL_AUX] >= 0.5;

	controlYaw = armed && mode == STAB && controls[RC_CHANNEL_YAW] == 0;
	if (!controlYaw) {
		yawTarget = attitude.getYaw();
	}

	if (mode == ACRO) {
		ratesTarget.x = controls[RC_CHANNEL_ROLL] * ROLLRATE_MAX;
		ratesTarget.y = -controls[RC_CHANNEL_PITCH] * PITCHRATE_MAX; // up pitch stick means tilt clockwise in frd
		ratesTarget.z = controls[RC_CHANNEL_YAW] * YAWRATE_MAX;

	} else if (mode == STAB) {
		attitudeTarget = Quaternion::fromEulerZYX(
			controls[RC_CHANNEL_ROLL] * MAX_TILT,
			-controls[RC_CHANNEL_PITCH] * MAX_TILT,
			yawTarget); // attitude.getYaw());
		ratesTarget.z = controls[RC_CHANNEL_YAW] * YAWRATE_MAX;
	}
}

static void controlAttitude()
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
	if (!isfinite(angle)) {
		// not enough precision to calculate
		Serial.println("angle is nan, skip");
		return;
	}

	Vector ratesTargetDir = Vector::angularRatesBetweenVectors(upTarget, upActual);
	ratesTargetDir.normalize();

	if (!ratesTargetDir.finite()) {
		Serial.println("ratesTargetDir is nan, skip");
		// std::cout << "angle is nan" << std::endl;
		ratesTarget = Vector(0, 0, 0);
		return;
	}

	ratesTarget.x = rollPID.update(ratesTargetDir.x * angle, dt);
	ratesTarget.y = pitchPID.update(ratesTargetDir.y * angle, dt);

	if (controlYaw) {
		ratesTarget.z = yawPID.update(yawTarget - attitude.getYaw(), dt); // WARNING:
	}

	if (!ratesTarget.finite()) {
		Serial.print("ratesTarget: "); Serial.println(ratesTarget);
		Serial.print("ratesTargetDir: "); Serial.println(ratesTargetDir);
		Serial.print("attitudeTarget: "); Serial.println(attitudeTarget);
		Serial.print("attitude: "); Serial.println(attitude);
		Serial.print("upActual: "); Serial.println(upActual);
		Serial.print("upTarget: "); Serial.println(upTarget);
		Serial.print("angle: "); Serial.println(angle);
		Serial.print("dt: "); Serial.println(dt);
	}

	// std::cout << "rsp: " << ratesTarget.x << " " << ratesTarget.y << std::endl;
}

static void controlAttitudeAlter()
{
	if (!armed) {
		rollPID.reset();
		pitchPID.reset();
		yawPID.reset();
		return;
	}
	Vector target = attitudeTarget.toEulerZYX();
	Vector att = attitude.toEulerZYX();

	ratesTarget.x = rollPID.update(target.x - att.x, dt);
	ratesTarget.y = pitchPID.update(target.y - att.y, dt);

	if (controlYaw) {
		ratesTarget.z = yawPID.update(target.z - att.z, dt); // WARNING:
	}
}

// passthrough mode
static void controlManual()
{
	if (controls[RC_CHANNEL_THROTTLE] < 0.1) {
		memset(motors, 0, sizeof(motors));
		return;
	}

	thrustTarget = controls[RC_CHANNEL_THROTTLE]; // collective thrust

	torqueTarget = ratesTarget * 0.01;

	motors[MOTOR_FRONT_LEFT] = thrustTarget + torqueTarget.y + torqueTarget.x - torqueTarget.z;
	motors[MOTOR_FRONT_RIGHT] = thrustTarget + torqueTarget.y - torqueTarget.x + torqueTarget.z;
	motors[MOTOR_REAR_LEFT] = thrustTarget - torqueTarget.y + torqueTarget.x + torqueTarget.z;
	motors[MOTOR_REAR_RIGHT] = thrustTarget - torqueTarget.y - torqueTarget.x - torqueTarget.z;

	if (!isfinite(motors[0]) || !isfinite(motors[1]) || !isfinite(motors[2]) || !isfinite(motors[3])) {
		Serial.println("motors are nan");
	}

	motors[0] = constrain(motors[0], 0, 1);
	motors[1] = constrain(motors[1], 0, 1);
	motors[2] = constrain(motors[2], 0, 1);
	motors[3] = constrain(motors[3], 0, 1);
}

static void controlRate()
{
	if (!armed) { // TODO: too rough
		memset(motors, 0, sizeof(motors));
		rollRatePID.reset();
		pitchRatePID.reset();
		yawRatePID.reset();
		return;
	}

	// collective thrust is throttle * 4
	thrustTarget = controls[RC_CHANNEL_THROTTLE]; // WARNING:

	ratesFiltered = rates * 0.8 + ratesFiltered * 0.2; // cut-off frequency 40 Hz

	torqueTarget.x = rollRatePID.update(ratesTarget.x - ratesFiltered.x, dt); // un-normalized "torque"
	torqueTarget.y = pitchRatePID.update(ratesTarget.y - ratesFiltered.y, dt);
	torqueTarget.z = yawRatePID.update(ratesTarget.z - ratesFiltered.z, dt);

	if (!torqueTarget.finite()) {
		Serial.print("torqueTarget: "); Serial.println(torqueTarget);
		Serial.print("ratesTarget: "); Serial.println(ratesTarget);
		Serial.print("rates: "); Serial.println(rates);
		Serial.print("dt: "); Serial.println(dt);
	}

	motors[MOTOR_FRONT_LEFT] = thrustTarget + torqueTarget.y + torqueTarget.x - torqueTarget.z;
	motors[MOTOR_FRONT_RIGHT] = thrustTarget + torqueTarget.y - torqueTarget.x + torqueTarget.z;
	motors[MOTOR_REAR_LEFT] = thrustTarget - torqueTarget.y + torqueTarget.x + torqueTarget.z;
	motors[MOTOR_REAR_RIGHT] = thrustTarget - torqueTarget.y - torqueTarget.x - torqueTarget.z;

	//indicateSaturation();
	// desaturate(motors[0], motors[1], motors[2], motors[3]);

	// constrain and reverse, multiple by -1 if reversed
	motors[0] = constrain(motors[0], 0, 1);
	motors[1] = constrain(motors[1], 0, 1);
	motors[2] = constrain(motors[2], 0, 1);
	motors[3] = constrain(motors[3], 0, 1);
}

void desaturate(float& a, float& b, float& c, float& d)
{
	float m = max(max(a, b), max(c, d));
	if (m > 1) {
		float diff = m - 1;
		a -= diff;
		b -= diff;
		c -= diff;
		d -= diff;
	}
	m = min(min(a, b), min(c, d));
	if (m < 0) {
		a -= m;
		b -= m;
		c -= m;
		d -= m;
	}
}

static bool motorsSaturation = false;

static inline void indicateSaturation() {
	bool sat = motors[0] > 1 || motors[1] > 1 || motors[2] > 1 || motors[3] > 1 ||
	           motors[0] < 0 || motors[1] < 0 || motors[2] < 0 || motors[3] < 0;
	if (motorsSaturation != sat) {
		motorsSaturation = sat;
		setLED(motorsSaturation);
	}
}
