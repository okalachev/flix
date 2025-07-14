// Copyright (c) 2024 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Fail-safe functions

#define RC_LOSS_TIMEOUT 0.2
#define DESCEND_TIME 3.0 // time to descend from full throttle to zero

extern double controlTime;
extern float controlRoll, controlPitch, controlThrottle, controlYaw;

void failsafe() {
	armingFailsafe();
	rcLossFailsafe();
}

// Prevent arming without zero throttle input
void armingFailsafe() {
	static double zeroThrottleTime;
	static double armingTime;
	if (!armed) armingTime = t; // stores the last time when the drone was disarmed, therefore contains arming time
	if (controlTime > 0 && controlThrottle < 0.05) zeroThrottleTime = controlTime;
	if (armingTime - zeroThrottleTime > 0.1) armed = false; // prevent arming if there was no zero throttle for 0.1 sec
}

// RC loss failsafe
void rcLossFailsafe() {
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
