// Copyright (c) 2024 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Fail-safe functions

extern float controlTime;
extern float voltage, voltageLow;

float rcLossTimeout = 1;
float descendTime = 10;
float disarmTilt = radians(120);
float lowBatteryTime = 3; // time the voltage should stay low to trigger landing, s
bool batteryLow = false; // low battery landing is in progress

void failsafe() {
	rcLossFailsafe();
	lowBatteryFailsafe();
	autoFailsafe();
	tiltFailsafe();
}

// RC loss failsafe
void rcLossFailsafe() {
	if (!armed) return;
	if (t - controlTime > rcLossTimeout) {
		descend();
	}
}

// Refuse arming and land when the battery is low (PWR_VOLT_LOW); inactive if the voltage is not measured
void lowBatteryFailsafe() {
	static bool wasArmed = false;
	static float lowSince = NAN;
	bool low = voltage > 0 && voltage <= voltageLow;

	if (armed && !wasArmed && low) {
		armed = false;
		print("Battery low (%.2f V), arming refused\n", voltage);
	}
	wasArmed = armed;

	if (!armed) {
		batteryLow = false;
		lowSince = NAN;
		return;
	}

	if (!low) {
		lowSince = NAN; // ignore short voltage sags
	} else if (isnan(lowSince)) {
		lowSince = t;
	} else if (!batteryLow && t - lowSince > lowBatteryTime) {
		batteryLow = true;
		print("Battery low (%.2f V), landing\n", voltage);
	}

	if (batteryLow && !(t - controlTime > rcLossTimeout)) descend(); // RC loss failsafe is already descending otherwise
}

// Smooth descend on RC loss or low battery
void descend() {
	mode = AUTO;
	attitudeTarget = Quaternion();
	thrustTarget -= dt / descendTime;
	if (thrustTarget < 0) {
		thrustTarget = 0;
		armed = false;
	}
}

// Allow pilot to interrupt automatic flight
void autoFailsafe() {
	static float roll, pitch, yaw, throttle;
	if (abs(roll - controlRoll) > 0.05 || abs(pitch - controlPitch) > 0.05 || abs(yaw - controlYaw) > 0.05 || abs(throttle - controlThrottle) > 0.05) {
		// controls changed and mode switch is not configured
		if (mode == AUTO && invalid(controlMode)) mode = STAB; // regain control by the pilot
	}
	roll = controlRoll;
	pitch = controlPitch;
	yaw = controlYaw;
	throttle = controlThrottle;
}

// Disarm if tilted too much
void tiltFailsafe() {
	if (!armed) return;
	if (mode != STAB) return;

	Vector up = Quaternion::rotateVector(Vector(0, 0, 1), attitude);
	float tilt = acos(up.z);
	if (disarmTilt && tilt > disarmTilt) {
		armed = false;
	}
}
