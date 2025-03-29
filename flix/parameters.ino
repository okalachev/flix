// Copyright (c) 2024 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Parameters storage in flash memory

#include <Preferences.h>

extern float channelNeutral[16];
extern float channelMax[16];
extern float mavlinkControlScale;

Preferences storage;

struct Parameter {
	const char *name;
	float *variable;
	float value; // cache
};

Parameter parameters[] = {
	// control
	{"ROLLRATE_P", &rollRatePID.p},
	{"ROLLRATE_I", &rollRatePID.i},
	{"ROLLRATE_D", &rollRatePID.d},
	{"ROLLRATE_I_LIM", &rollRatePID.windup},
	{"PITCHRATE_P", &pitchRatePID.p},
	{"PITCHRATE_I", &pitchRatePID.i},
	{"PITCHRATE_D", &pitchRatePID.d},
	{"PITCHRATE_I_LIM", &pitchRatePID.windup},
	{"YAWRATE_P", &yawRatePID.p},
	{"YAWRATE_I", &yawRatePID.i},
	{"YAWRATE_D", &yawRatePID.d},
	{"ROLL_P", &rollPID.p},
	{"ROLL_I", &rollPID.i},
	{"ROLL_D", &rollPID.d},
	{"PITCH_P", &pitchPID.p},
	{"PITCH_I", &pitchPID.i},
	{"PITCH_D", &pitchPID.d},
	{"YAW_P", &yawPID.p},
	{"PITCHRATE_MAX", &maxRate.y},
	{"ROLLRATE_MAX", &maxRate.x},
	{"YAWRATE_MAX", &maxRate.z},
	{"TILT_MAX", &tiltMax},
	// imu
	{"ACC_BIAS_X", &accBias.x},
	{"ACC_BIAS_Y", &accBias.y},
	{"ACC_BIAS_Z", &accBias.z},
	{"ACC_SCALE_X", &accScale.x},
	{"ACC_SCALE_Y", &accScale.y},
	{"ACC_SCALE_Z", &accScale.z},
	// rc
	{"RC_NEUTRAL_0", &channelNeutral[0]},
	{"RC_NEUTRAL_1", &channelNeutral[1]},
	{"RC_NEUTRAL_2", &channelNeutral[2]},
	{"RC_NEUTRAL_3", &channelNeutral[3]},
	{"RC_NEUTRAL_4", &channelNeutral[4]},
	{"RC_NEUTRAL_5", &channelNeutral[5]},
	{"RC_NEUTRAL_6", &channelNeutral[6]},
	{"RC_NEUTRAL_7", &channelNeutral[7]},
	{"RC_MAX_0", &channelMax[0]},
	{"RC_MAX_1", &channelMax[1]},
	{"RC_MAX_2", &channelMax[2]},
	{"RC_MAX_3", &channelMax[3]},
	{"RC_MAX_4", &channelMax[4]},
	{"RC_MAX_5", &channelMax[5]},
	{"RC_MAX_6", &channelMax[6]},
	{"RC_MAX_7", &channelMax[7]},
#if WIFI_ENABLED
	// MAVLink
	{"MAV_CTRL_SCALE", &mavlinkControlScale},
#endif
};

void setupParameters() {
	storage.begin("flix", false);
	// Read parameters from storage
	for (auto &parameter : parameters) {
		if (!storage.isKey(parameter.name)) {
			storage.putFloat(parameter.name, *parameter.variable);
		}
		*parameter.variable = storage.getFloat(parameter.name, *parameter.variable);
		parameter.value = *parameter.variable;
	}
}

int parametersCount() {
	return sizeof(parameters) / sizeof(parameters[0]);
}

const char *getParameterName(int index) {
	if (index < 0 || index >= parametersCount()) return "";
	return parameters[index].name;
}

float getParameter(int index) {
	if (index < 0 || index >= parametersCount()) return NAN;
	return *parameters[index].variable;
}

float getParameter(const char *name) {
	for (auto &parameter : parameters) {
		if (strcmp(parameter.name, name) == 0) {
			return *parameter.variable;
		}
	}
	return NAN;
}

bool setParameter(const char *name, const float value) {
	for (auto &parameter : parameters) {
		if (strcmp(parameter.name, name) == 0) {
			*parameter.variable = value;
			return true;
		}
	}
	return false;
}

void syncParameters() {
	static double lastSync = 0;
	if (t - lastSync < 1) return; // sync once per second
	if (motorsActive()) return; // don't use flash while flying, it may cause a delay
	lastSync = t;

	for (auto &parameter : parameters) {
		if (parameter.value == *parameter.variable) continue;
		if (isnan(parameter.value) && isnan(*parameter.variable)) continue; // handle NAN != NAN
		storage.putFloat(parameter.name, *parameter.variable);
		parameter.value = *parameter.variable;
	}
}

void printParameters() {
	for (auto &parameter : parameters) {
		print("%s = %g\n", parameter.name, *parameter.variable);
	}
}

void resetParameters() {
	storage.clear();
	ESP.restart();
}
