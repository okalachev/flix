// Copyright (c) 2024 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Fail-safe functions

#define RC_LOSS_TIMEOUT 0.2
#define DESCEND_TIME 3.0 // time to descend from full throttle to zero

extern double controlsTime;
extern int rollChannel, pitchChannel, throttleChannel, yawChannel;

void failsafe() {
	armingFailsafe();
	rcLossFailsafe();
}

// Prevent arming without zero throttle input
void armingFailsafe() {
	static double zeroThrottleTime;
	static double armingTime;
	if (!armed) armingTime = t; // stores the last time when the drone was disarmed, therefore contains arming time
	if (controlsTime > 0 && controls[throttleChannel] < 0.05) zeroThrottleTime = controlsTime;
	if (armingTime - zeroThrottleTime > 0.1) armed = false; // prevent arming if there was no zero throttle for 0.1 sec
}

// RC loss failsafe
void rcLossFailsafe() {
	if (t - controlsTime > RC_LOSS_TIMEOUT) {
		descend();
	}
}

// Smooth descend on RC lost
void descend() {
	mode = STAB;
	controls[rollChannel] = 0;
	controls[pitchChannel] = 0;
	controls[yawChannel] = 0;
	controls[throttleChannel] -= dt / DESCEND_TIME;
	if (controls[throttleChannel] < 0) controls[throttleChannel] = 0;
}
