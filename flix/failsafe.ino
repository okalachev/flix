// Copyright (c) 2024 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Fail-safe for RC loss

#define RC_LOSS_TIMEOUT 0.2
#define DESCEND_TIME 3.0 // time to descend from full throttle to zero

extern float controlsTime;

void failsafe() {
	if (t - controlsTime > RC_LOSS_TIMEOUT) {
		descend();
	}
}

void descend() {
	// Smooth descend on RC lost
	mode = STAB;
	controls[RC_CHANNEL_ROLL] = 0;
	controls[RC_CHANNEL_PITCH] = 0;
	controls[RC_CHANNEL_YAW] = 0;
	controls[RC_CHANNEL_THROTTLE] -= dt / DESCEND_TIME;
	if (controls[RC_CHANNEL_THROTTLE] < 0) controls[RC_CHANNEL_THROTTLE] = 0;
}
