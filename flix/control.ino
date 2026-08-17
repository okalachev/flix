// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Flight control

#include "vector.h"
#include "quaternion.h"
#include "pid.h"
#include "filter.h"
#include "util.h"

const int RAW = 0, ACRO = 1, STAB = 2, AUTO = 3; // flight modes
int mode = STAB;
bool armed = false;

Quaternion attitudeTarget;
Vector ratesTarget;
Vector ratesExtra; // feedforward rates
Vector torqueTarget;
float thrustTarget;

PID rollRatePID(0.05, 0.2, 0.001, 0.3, 0.2);
PID pitchRatePID(0.05, 0.2, 0.001, 0.3, 0.2);
PID yawRatePID(0.3, 0, 0, 0.3);
PID rollPID(6);
PID pitchPID(6);
PID yawPID(3);
Vector maxRate(radians(360), radians(360), radians(360));
float tiltMax = radians(30);
int flightModes[] = {STAB, STAB, STAB}; // map for rc mode switch

extern const int MOT_RL, MOT_RR, MOT_FR, MOT_FL;
extern float controlRoll, controlPitch, controlThrottle, controlYaw, controlMode;

void control() {
	interpretControls();
	failsafe();
	controlAttitude();
	controlRates();
	controlTorque();
}

void interpretControls() {
	if (controlMode < 0.25) mode = flightModes[0];
	else if (controlMode <= 0.75) mode = flightModes[1];
	else if (controlMode > 0.75) mode = flightModes[2];

	if (mode == AUTO) return; // pilot is not effective in AUTO mode

	if (controlThrottle < 0.05 && controlYaw > 0.95) armed = true; // arm gesture
	if (controlThrottle < 0.05 && controlYaw < -0.95) armed = false; // disarm gesture

	if (abs(controlYaw) < 0.1) controlYaw = 0; // yaw dead zone

	thrustTarget = controlThrottle;

	if (mode == STAB) {
		float yawTarget = attitudeTarget.getYaw();
		if (!armed || invalid(yawTarget) || controlYaw != 0) yawTarget = attitude.getYaw(); // reset yaw target
		attitudeTarget = Quaternion::fromEuler(Vector(controlRoll * tiltMax, controlPitch * tiltMax, yawTarget));
		ratesExtra = Vector(0, 0, -controlYaw * maxRate.z); // positive yaw stick means clockwise rotation in FLU
	}

	if (mode == ACRO) {
		attitudeTarget.invalidate(); // skip attitude control
		ratesTarget.x = controlRoll * maxRate.x;
		ratesTarget.y = controlPitch * maxRate.y;
		ratesTarget.z = -controlYaw * maxRate.z; // positive yaw stick means clockwise rotation in FLU
	}

	if (mode == RAW) { // direct torque control
		attitudeTarget.invalidate(); // skip attitude control
		ratesTarget.invalidate(); // skip rate control
		torqueTarget = Vector(controlRoll, controlPitch, -controlYaw) * 0.1;
	}
}

void controlAttitude() {
	if (!armed || attitudeTarget.invalid() || thrustTarget < 0.1) return; // skip attitude control

	const Vector up(0, 0, 1);
	Vector upActual = Quaternion::rotateVector(up, attitude);
	Vector upTarget = Quaternion::rotateVector(up, attitudeTarget);

	Vector error = Vector::rotationVectorBetween(upTarget, upActual);

	ratesTarget.x = rollPID.update(error.x) + ratesExtra.x;
	ratesTarget.y = pitchPID.update(error.y) + ratesExtra.y;

	float yawError = wrapAngle(attitudeTarget.getYaw() - attitude.getYaw());
	ratesTarget.z = yawPID.update(yawError) + ratesExtra.z;
}


void controlRates() {
	if (!armed || ratesTarget.invalid() || thrustTarget < 0.1) return; // skip rates control

	Vector error = ratesTarget - rates;

	// Calculate desired torque, where 0 - no torque, 1 - maximum possible torque
	torqueTarget.x = rollRatePID.update(error.x);
	torqueTarget.y = pitchRatePID.update(error.y);
	torqueTarget.z = yawRatePID.update(error.z);
}

void controlTorque() {
	if (!torqueTarget.valid()) return; // skip torque control

	if (!armed) {
		memset(motors, 0, sizeof(motors)); // stop motors if disarmed
		return;
	}

	if (thrustTarget < 0.1) {
		motors[0] = 0.1; // idle thrust
		motors[1] = 0.1;
		motors[2] = 0.1;
		motors[3] = 0.1;
		return;
	}

	motors[MOT_FL] = thrustTarget + torqueTarget.x - torqueTarget.y + torqueTarget.z;
	motors[MOT_FR] = thrustTarget - torqueTarget.x - torqueTarget.y - torqueTarget.z;
	motors[MOT_RL] = thrustTarget + torqueTarget.x + torqueTarget.y - torqueTarget.z;
	motors[MOT_RR] = thrustTarget - torqueTarget.x + torqueTarget.y + torqueTarget.z;

	// Prioritize angle control over thrust control
	desaturate(motors[MOT_FL], motors[MOT_FR], motors[MOT_RL], motors[MOT_RR]);

	motors[0] = constrain(motors[0], 0, 1);
	motors[1] = constrain(motors[1], 0, 1);
	motors[2] = constrain(motors[2], 0, 1);
	motors[3] = constrain(motors[3], 0, 1);
}

void desaturate(float& a, float& b, float& c, float& d) {
	float maxThrust = max(max(a, b), max(c, d));
	if (maxThrust > 1) {
		float diff = maxThrust - 1;
		a -= diff;
		b -= diff;
		c -= diff;
		d -= diff;
	}
}

const char* getModeName() {
	switch (mode) {
		case RAW: return "RAW";
		case ACRO: return "ACRO";
		case STAB: return "STAB";
		case AUTO: return "AUTO";
		default: return "UNKNOWN";
	}
}
