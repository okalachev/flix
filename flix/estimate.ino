// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

#include "quaternion.h"
#include "vector.h"

#define ONE_G 9.807f
#define ACC_MIN 0.9f
#define ACC_MAX 1.1f
#define WEIGHT_ACC 0.5f

void estimate()
{
	if (dt == 0) {
		return;
	}

	// applying gyro
	attitude *= Quaternion::fromAngularRates(rates * dt);
	attitude.normalize();

	// test should we apply acc
	float accNorm = acc.norm();
	if (accNorm < ACC_MIN * ONE_G || accNorm > ACC_MAX * ONE_G) {
		// use acc only when we're not accelerating
		return;
	}

	Vector up = attitude.rotate(Vector(0, 0, -1));
	Vector accCorrDirection = Vector::angularRatesBetweenVectors(acc, up);
	accCorrDirection.normalize();

	if (!accCorrDirection.finite()) {
		return;
	}

	Vector accCorr = accCorrDirection * Vector::angleBetweenVectors(up, acc) * dt * WEIGHT_ACC;

	if (!accCorr.finite()) {
		return; // TODO
	}

	attitude *= Quaternion::fromAngularRates(accCorr);
	attitude.normalize();

	if (!attitude.finite()) {
		Serial.print("dt "); Serial.println(dt, 15);
		Serial.print("up "); Serial.println(up);
		Serial.print("acc "); Serial.println(acc);
		Serial.print("acc norm "); Serial.println(acc.norm());
		Serial.print("upp norm "); Serial.println(up.norm());
		Serial.print("acc dot up "); Serial.println(Vector::dot(up, acc), 15);
		Serial.print("acc cor ang "); Serial.println(Vector::angleBetweenVectors(up, acc), 15);
		Serial.print("acc corr dir "); Serial.println(accCorrDirection);
		Serial.print("acc cor "); Serial.println(accCorr);
		Serial.print("att "); Serial.println(attitude);
	}
}

void signalizeHorizontality()
{
	float angle = Vector::angleBetweenVectors(attitude.rotate(Vector(0, 0, -1)), Vector(0, 0, -1));
	setLED(angle < 15 * DEG_TO_RAD);
}
