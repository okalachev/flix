// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Attitude estimation from gyro and accelerometer

#include "quaternion.h"
#include "vector.h"

#define ONE_G 9.807f
#define ACC_MIN 0.9f
#define ACC_MAX 1.1f
#define WEIGHT_ACC 0.5f

void estimate()
{
	applyGyro();
	applyAcc();
	signalizeHorizontality();
}

void applyGyro()
{
	// applying gyro
	attitude *= Quaternion::fromAngularRates(rates * dt);
	attitude.normalize();
}

void applyAcc()
{
	// test should we apply accelerometer gravity correction
	float accNorm = acc.norm();
	if (accNorm < ACC_MIN * ONE_G || accNorm > ACC_MAX * ONE_G) {
		// use accelerometer only when we're not accelerating
		return;
	}

	// calculate accelerometer correction
	Vector up = attitude.rotate(Vector(0, 0, -1));
	Vector correction = Vector::angularRatesBetweenVectors(acc, up) * dt * WEIGHT_ACC;

	// apply correction
	attitude *= Quaternion::fromAngularRates(correction);
	attitude.normalize();
}

void signalizeHorizontality()
{
	float angle = Vector::angleBetweenVectors(attitude.rotate(Vector(0, 0, -1)), Vector(0, 0, -1));
	setLED(angle < 15 * DEG_TO_RAD);
}
