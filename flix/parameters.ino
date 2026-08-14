// Copyright (c) 2024 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Parameters storage in flash memory

#include <Preferences.h>
#include "util.h"

extern int channelZero[16], channelMax[16];
extern int rollChannel, pitchChannel, throttleChannel, yawChannel, armedChannel, modeChannel;
extern int rcRxPin, voltagePin;
extern int wifiMode, wifiLongRange, wifiBroadcast, udpLocalPort, udpRemotePort, espnowChannel;
extern float rcLossTimeout, descendTime, disarmTilt;
extern float voltageScale;
extern LowPassFilter<float> voltageFilter;

#include "config.h"

Preferences storage;

struct Parameter {
	const char *name; // max length is 15
	Value value; // pointer to the variable
	float initial; // default value
	float cache; // what's stored in flash
	void (*callback)(); // called after parameter change
	Parameter(const char *name, Value value, void (*callback)() = nullptr) : name(name), value(value), callback(callback) {};
};

Parameter parameters[] = {
	// control
	{"CTL_R_RATE_P", &rollRatePID.p},
	{"CTL_R_RATE_I", &rollRatePID.i},
	{"CTL_R_RATE_D", &rollRatePID.d},
	{"CTL_R_RATE_WU", &rollRatePID.windup},
	{"CTL_R_RATE_D_A", &rollRatePID.lpf.alpha},
	{"CTL_P_RATE_P", &pitchRatePID.p},
	{"CTL_P_RATE_I", &pitchRatePID.i},
	{"CTL_P_RATE_D", &pitchRatePID.d},
	{"CTL_P_RATE_WU", &pitchRatePID.windup},
	{"CTL_P_RATE_D_A", &pitchRatePID.lpf.alpha},
	{"CTL_Y_RATE_P", &yawRatePID.p},
	{"CTL_Y_RATE_I", &yawRatePID.i},
	{"CTL_Y_RATE_D", &yawRatePID.d},
	{"CTL_Y_RATE_WU", &yawRatePID.windup},
	{"CTL_Y_RATE_D_A", &yawRatePID.lpf.alpha},
	{"CTL_R_P", &rollPID.p},
	{"CTL_R_I", &rollPID.i},
	{"CTL_R_D", &rollPID.d},
	{"CTL_P_P", &pitchPID.p},
	{"CTL_P_I", &pitchPID.i},
	{"CTL_P_D", &pitchPID.d},
	{"CTL_Y_P", &yawPID.p},
	{"CTL_P_RATE_MAX", &maxRate.y},
	{"CTL_R_RATE_MAX", &maxRate.x},
	{"CTL_Y_RATE_MAX", &maxRate.z},
	{"CTL_TILT_MAX", &tiltMax},
	{"CTL_FLT_MODE_0", &flightModes[0]},
	{"CTL_FLT_MODE_1", &flightModes[1]},
	{"CTL_FLT_MODE_2", &flightModes[2]},
	// imu
	{"IMU_MODEL", &imuModel},
	{"IMU_BUS", &imuBus},
	{"IMU_PIN_SCK", &imuSckPin},
	{"IMU_PIN_MISO", &imuMisoPin},
	{"IMU_PIN_MOSI", &imuMosiPin},
	{"IMU_PIN_CS", &imuCsPin},
	{"IMU_PIN_SDA", &imuSdaPin},
	{"IMU_PIN_SCL", &imuSclPin},
	{"IMU_PIN_INT", &imuIntPin},
	{"IMU_ROT_ROLL", &imuRotation.x},
	{"IMU_ROT_PITCH", &imuRotation.y},
	{"IMU_ROT_YAW", &imuRotation.z},
	{"IMU_ACC_BIAS_X", &accBias.x},
	{"IMU_ACC_BIAS_Y", &accBias.y},
	{"IMU_ACC_BIAS_Z", &accBias.z},
	{"IMU_ACC_SCALE_X", &accScale.x},
	{"IMU_ACC_SCALE_Y", &accScale.y},
	{"IMU_ACC_SCALE_Z", &accScale.z},
	{"IMU_GYRO_BIAS_A", &gyroBiasFilter.alpha},
	// estimate
	{"EST_ACC_WEIGHT", &accWeight},
	{"EST_LVL_WEIGHT", &levelWeight},
	{"EST_RATES_LPF_A", &ratesFilter.alpha},
	{"EST_RATES_NF_F", &ratesNotch.frequency, setupEstimate},
	{"EST_RATES_NF_BW", &ratesNotch.bandwidth, setupEstimate},
	// motors
	{"MOT_PIN_FL", &motorPins[MOTOR_FRONT_LEFT], setupMotors},
	{"MOT_PIN_FR", &motorPins[MOTOR_FRONT_RIGHT], setupMotors},
	{"MOT_PIN_RL", &motorPins[MOTOR_REAR_LEFT], setupMotors},
	{"MOT_PIN_RR", &motorPins[MOTOR_REAR_RIGHT], setupMotors},
	{"MOT_PWM_FREQ", &pwmFrequency, setupMotors},
	{"MOT_PWM_RES", &pwmResolution, setupMotors},
	{"MOT_PWM_STOP", &pwmStop},
	{"MOT_PWM_MIN", &pwmMin},
	{"MOT_PWM_MAX", &pwmMax},
	// rc
	{"RC_RX_PIN", &rcRxPin, setupRC},
	{"RC_ZERO_0", &channelZero[0]},
	{"RC_ZERO_1", &channelZero[1]},
	{"RC_ZERO_2", &channelZero[2]},
	{"RC_ZERO_3", &channelZero[3]},
	{"RC_ZERO_4", &channelZero[4]},
	{"RC_ZERO_5", &channelZero[5]},
	{"RC_ZERO_6", &channelZero[6]},
	{"RC_ZERO_7", &channelZero[7]},
	{"RC_MAX_0", &channelMax[0]},
	{"RC_MAX_1", &channelMax[1]},
	{"RC_MAX_2", &channelMax[2]},
	{"RC_MAX_3", &channelMax[3]},
	{"RC_MAX_4", &channelMax[4]},
	{"RC_MAX_5", &channelMax[5]},
	{"RC_MAX_6", &channelMax[6]},
	{"RC_MAX_7", &channelMax[7]},
	{"RC_ROLL", &rollChannel},
	{"RC_PITCH", &pitchChannel},
	{"RC_THROTTLE", &throttleChannel},
	{"RC_YAW", &yawChannel},
	{"RC_MODE", &modeChannel},
	// wifi
	{"WIFI_MODE", &wifiMode},
	{"WIFI_PORT_LOC", &udpLocalPort},
	{"WIFI_PORT_REM", &udpRemotePort},
	{"WIFI_LONG_RANGE", &wifiLongRange},
	{"WIFI_BROADCAST", &wifiBroadcast},
	// espnow
	{"ESPNOW_CHANNEL", &espnowChannel},
	// mavlink
	{"MAV_SYS_ID", &mavlinkSysId},
	{"MAV_RATE_SLOW", &telemetrySlow.rate},
	{"MAV_RATE_ATT", &telemetryAttitude.rate},
	{"MAV_RATE_RC", &telemetryRC.rate},
	{"MAV_RATE_MOT", &telemetryMotors.rate},
	{"MAV_RATE_IMU", &telemetryIMU.rate},
	{"MAV_RATE_TOPIC", &telemetryTopic.rate},
	// log
	{"LOG_MEMORY", &logMemory, setupLog},
	{"LOG_USAGE", &logUsage, setupLog},
	{"LOG_RATE_000", &logTopics[0].throttle},
	{"LOG_RATE_001", &logTopics[1].throttle},
	{"LOG_RATE_002", &logTopics[2].throttle},
	{"LOG_RATE_003", &logTopics[3].throttle},
	{"LOG_RATE_004", &logTopics[4].throttle},
	{"LOG_RATE_005", &logTopics[5].throttle},
	{"LOG_RATE_006", &logTopics[6].throttle},
	{"LOG_RATE_007", &logTopics[7].throttle},
	{"LOG_RATE_008", &logTopics[8].throttle},
	{"LOG_RATE_009", &logTopics[9].throttle},
	{"LOG_RATE_010", &logTopics[10].throttle},
	{"LOG_RATE_011", &logTopics[11].throttle},
	// power
	{"PWR_VOLT_PIN", &voltagePin, setupPower},
	{"PWR_VOLT_SCALE", &voltageScale},
	{"PWR_VOLT_LPF_A", &voltageFilter.alpha},
	// safety
	{"SF_RC_LOSS_TIME", &rcLossTimeout},
	{"SF_DESCEND_TIME", &descendTime},
	{"SF_DISARM_TILT", &disarmTilt},
};

void setupParameters() {
	print("Setup parameters\n");
	setDefaults();
	storage.begin("flix");
	// Read parameters from storage
	for (auto &parameter : parameters) {
		parameter.initial = parameter.value.get();
		if (storage.isKey(parameter.name)) {
			parameter.value.set(storage.getFloat(parameter.name));
		}
		parameter.cache = parameter.value.get();
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
	return parameters[index].value.get();
}

float getParameter(const char *name) {
	for (auto &parameter : parameters) {
		if (strcasecmp(parameter.name, name) == 0) {
			return parameter.value.get();
		}
	}
	return NAN;
}

bool setParameter(const char *name, const float value) {
	for (auto &parameter : parameters) {
		if (strcasecmp(parameter.name, name) == 0) {
			bool success = parameter.value.set(value);
			if (parameter.callback) parameter.callback();
			return success;
		}
	}
	return false;
}

void syncParameters() {
	static Rate rate(1);
	if (!rate) return; // sync once per second
	if (motorsActive()) return; // don't use flash while flying, it may cause a delay

	for (auto &parameter : parameters) {
		if (floatEquals(parameter.value.get(), parameter.cache)) continue; // no change

		storage.putFloat(parameter.name, parameter.value.get());
		parameter.cache = parameter.value.get(); // update cache
	}
}

void printParameters(const char *filter) {
	print("Name             Value          [Default]\n");
	for (auto &parameter : parameters) {
		if (strncasecmp(parameter.name, filter, strlen(filter))) continue;

		if (floatEquals(parameter.value.get(), parameter.initial)) { // parameter changed
			print("%-15s  %-13g\n", parameter.name, parameter.value.get());
		} else {
			print("%-15s  %-13g  [%g]\n", parameter.name, parameter.value.get(), parameter.initial);
		}
	}
}

void resetParameters() {
	storage.clear();
	ESP.restart();
}
