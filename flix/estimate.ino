// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Attitude estimation from gyro and accelerometer

#include "quaternion.h"
#include "vector.h"
#include "lpf.h"
#include "util.h"

#define WEIGHT_ACC 0.5f
#define RATES_LFP_ALPHA 0.2 // cutoff frequency ~ 40 Hz

LowPassFilter<Vector> ratesFilter(RATES_LFP_ALPHA);

void estimate() {
	applyGyro();
	applyAcc();
}

void applyGyro() {
	// filter gyro to get angular rates
	rates = ratesFilter.update(gyro);

	// apply rates to attitude
	attitude = attitude.rotate(Quaternion::fromAngularRates(rates * dt));
}

void applyAcc() {
	// test should we apply accelerometer gravity correction
	float accNorm = acc.norm();
	bool landed = !motorsActive() && abs(accNorm - ONE_G) < ONE_G * 0.1f;

	if (!landed) return;

	// calculate accelerometer correction
	Vector up = attitude.rotateVector(Vector(0, 0, 1));
	Vector correction = Vector::angularRatesBetweenVectors(acc, up) * dt * WEIGHT_ACC;

	// apply correction
	attitude = attitude.rotate(Quaternion::fromAngularRates(correction));
}
