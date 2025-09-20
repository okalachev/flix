// Copyright (c) 2024 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Fail-safe functions

#define RC_LOSS_TIMEOUT 0.5
#define DESCEND_TIME 3.0 // time to descend from full throttle to zero

extern double controlTime;
extern float controlRoll, controlPitch, controlThrottle, controlYaw;

void failsafe() {
	rcLossFailsafe();
	autoFailsafe();
}

// RC loss failsafe
void rcLossFailsafe() {
	if (mode == AUTO) return;
	if (!armed) return;
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
	if (controlThrottle < 0) {
		armed = false;
		controlThrottle = 0;
	}
}

// Allow pilot to interrupt automatic flight
void autoFailsafe() {
	static float roll, pitch, yaw, throttle;
	if (roll != controlRoll || pitch != controlPitch || yaw != controlYaw || abs(throttle - controlThrottle) > 0.05) {
		// controls changed
		if (mode == AUTO) mode = STAB; // regain control by the pilot
	}
	roll = controlRoll;
	pitch = controlPitch;
	yaw = controlYaw;
	throttle = controlThrottle;
}
