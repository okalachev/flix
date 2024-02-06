// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// PID controller implementation

#pragma once

#include "lpf.h"

class PID {
public:
	float p = 0;
	float i = 0;
	float d = 0;
	float windup = 0;

	float derivative = 0;
	float integral = 0;

	LowPassFilter<float> lpf; // low pass filter for derivative term

	PID(float p, float i, float d, float windup = 0, float dAlpha = 1) : p(p), i(i), d(d), windup(windup), lpf(dAlpha) {};

	float update(float error, float dt) {
		integral += error * dt;

		if (isfinite(prevError) && dt > 0) {
			// calculate derivative if both dt and prevError are valid
			derivative = (error - prevError) / dt;

			// apply low pass filter to derivative
			derivative = lpf.update(derivative);
		}

		prevError = error;

		return p * error + constrain(i * integral, -windup, windup) + d * derivative; // PID
	}

	void reset() {
		prevError = NAN;
		integral = 0;
		derivative = 0;
	}

private:
	float prevError = NAN;
};
