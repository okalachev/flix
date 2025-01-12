// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Time related functions

float loopRate; // Hz

void step() {
	double now = micros() / 1000000.0;
	dt = now - t;
	t = now;

	if (!(dt > 0)) {
		dt = 0; // assume dt to be zero on first step and on reset
	}

	computeLoopRate();
}

void computeLoopRate() {
	static double windowStart = 0;
	static uint32_t rate = 0;
	rate++;
	if (t - windowStart >= 1) { // 1 second window
		loopRate = rate;
		windowStart = t;
		rate = 0;
	}
}
