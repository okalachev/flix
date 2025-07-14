// Copyright (c) 2024 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Fail-safe for RC loss

#define RC_LOSS_TIMEOUT 0.2
#define DESCEND_TIME 3.0 // time to descend from full throttle to zero

extern float controlTime;

// RC loss failsafe
void failsafe() {
	if (t - controlTime > RC_LOSS_TIMEOUT) {
		descend();
	}
}

// Smooth descend on RC lost
void descend() {
	mode = STAB;
	controlRoll = 0;
	controlPitch = 0;
	controlYaw = 0;
	controlThrottle -= dt / DESCEND_TIME;
	if (controlThrottle < 0) controlThrottle = 0;
}
