// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

#define FREQ_WINDOW 1

void step()
{
	float now = micros() / 1000000.0;
	dt = now - t; // dt is NAN on first step
	t = now;

	computeLoopFreq();
}

void computeLoopFreq()
{
	static float windowStart = 0;
	static uint32_t freq = 0;
	freq++;
	if (t - windowStart >= FREQ_WINDOW) {
		loopFreq = freq;
		windowStart = t;
		freq = 0;
	}
}
