#include "vector.h"
#include "quaternion.h"
#include "pid.h"
#include "util.h"

float mass = 0.065;
float motorThrust = 0.031 * ONE_G;
float positionWeight = 0.9;
float velocityWeight = 0.9;

PID velX_PID(1.0, 0.0, 0.02);
PID velY_PID(1.0, 0.0, 0.02);
PID velZ_PID(1.0, 0.0, 0.02);

// PID posX_PID(1.0, 0.0, 0.0);
// PID posY_PID(1.0, 0.0, 0.0);
// PID posZ_PID(1.0, 0.0, 0.0);

PID posX_PID(0.1, 0.0, 0.0);
PID posY_PID(0.1, 0.0, 0.0);
PID posZ_PID(0.1, 0.0, 0.0);

Vector acceleration;
Vector velocity;
Vector position;
Vector velocityTarget(NAN, NAN, NAN);
Vector positionTarget(NAN, NAN, NAN);

extern Quaternion attitudeTarget;
extern float thrustTarget;

void setupPosition() {
	// TODO: posY_PID = posX_PID;
	// posX_PID.copy(posY_PID);
	posY_PID.p = posX_PID.p;
	posY_PID.i = posX_PID.i;
	posY_PID.d = posX_PID.d;
}

void estimatePosition() {
	acceleration = Quaternion::rotateVector(acceleration, attitude) + Vector(0, 0, -ONE_G);
	velocity += acceleration * dt;
	position += velocity * dt;
}

void correctPosition(Vector& pos) {
	// position += (pos - position) * positionWeight;
	position = pos;
	// positionUpdated = true;

	if (invalid(positionTarget.x) || invalid(positionTarget.y)) return;

	attitudeTarget.setPitch(posX_PID.update(positionTarget.x - position.x));
	attitudeTarget.setRoll(posY_PID.update(positionTarget.y - position.y));
}

void correctVelocity(Vector& vel) {
	velocity += (vel - velocity) * velocityWeight;
}

void controlVelocity() {
	Vector accelerationTarget(
		velX_PID.update(velocityTarget.x - velocity.x),
		velY_PID.update(velocityTarget.y - velocity.y),
		velZ_PID.update(velocityTarget.z - velocity.z)
	);

	Vector thrustVector = (accelerationTarget + Vector(0, 0, ONE_G)) * mass;

	const Vector up(0, 0, 1);

	attitudeTarget = Quaternion::fromBetweenVectors(up, thrustVector);
	float maxThrust = motorThrust * 4; // 4 motors
	thrustTarget = thrustVector.norm() / maxThrust;
}

void controlPosition() {
	if (positionTarget.invalid()) return;

	velocityTarget = Vector(
		posX_PID.update(positionTarget.x - position.x),
		posY_PID.update(positionTarget.y - position.y),
		posZ_PID.update(positionTarget.z - position.z)
	);

	controlVelocity();
}

void controlPositionSimple() {
	if (positionTarget.invalid()) return;

	// straight attitude target control
	attitudeTarget.setPitch(posX_PID.update(positionTarget.x - position.x));
	attitudeTarget.setRoll(posY_PID.update(positionTarget.y - position.y));
}
