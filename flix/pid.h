// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// PID controller implementation

#pragma once

class PID
{
public:
	float p = 0;
	float i = 0;
	float d = 0;
	float windup = 0;

	float derivative = 0;
	float integral = 0;

	PID(float p, float i, float d, float windup = 0) : p(p), i(i), d(d), windup(windup) {};

	float update(float error, float dt)
	{
		integral += error * dt;
		if (isfinite(prevError) && dt > 0) {
			// calculate derivative if both dt and prevError are valid
			float _derivative = (error - prevError) / dt;
			derivative = derivative * 0.8 + 0.2 * _derivative; // lpf WARNING:
		}

		prevError = error;

		return p * error + constrain(i * integral, -windup, windup) + d * derivative; // PID
	}

	void reset()
	{
		prevError = NAN;
		integral = 0;
		derivative = 0;
	}

private:
	float prevError = NAN;
};
