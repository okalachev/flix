// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Logging subsystem

#include <vector>
#include "util.h"

int logMemory = -1; // 0 - RAM, 1 - PSRAM, -1 - disabled
float logUsage = 0.5; // fraction of free memory to use for log

struct LogValue {
	const char *name;
	Value value;
	float cache = NAN;
	bool exposed = false; // exposed to telemetry
	template<typename T> LogValue(const char *name, T value) : name(name), value(Value(value)) {}
};

LogValue logValues[] = {
	{"t", &t},
	{"loopRate", &loopRate},
	{"gyro.x", &gyro.x},
	{"gyro.y", &gyro.y},
	{"gyro.z", &gyro.z},
	{"acc.x", &acc.x},
	{"acc.y", &acc.y},
	{"acc.z", &acc.z},
	{"gyroBias.x", &gyroBias.x},
	{"gyroBias.y", &gyroBias.y},
	{"gyroBias.z", &gyroBias.z},
	{"rates.x", &rates.x},
	{"rates.y", &rates.y},
	{"rates.z", &rates.z},
	{"attitude.roll", []() { return attitude.getRoll(); }},
	{"attitude.pitch", []() { return attitude.getPitch(); }},
	{"attitude.yaw", []() { return attitude.getYaw(); }},
	{"controlTime", &controlTime},
	{"controlRoll", &controlRoll},
	{"controlPitch", &controlPitch},
	{"controlYaw", &controlYaw},
	{"controlThrottle", &controlThrottle},
	{"armed", &armed},
	{"mode", &mode},
	{"ratesTarget.x", &ratesTarget.x},
	{"ratesTarget.y", &ratesTarget.y},
	{"ratesTarget.z", &ratesTarget.z},
	{"attitudeTarget.roll", []() { return attitudeTarget.getRoll(); }},
	{"attitudeTarget.pitch", []() { return attitudeTarget.getPitch(); }},
	{"attitudeTarget.yaw", []() { return attitudeTarget.getYaw(); }},
	{"thrustTarget", &thrustTarget},
	{"motors[0]", &motors[0]},
	{"motors[1]", &motors[1]},
	{"motors[2]", &motors[2]},
	{"motors[3]", &motors[3]},
	{"voltage", &voltage},
	{"temp", &temperatureRead},
	{"imuTemp", []() { return imu->getTemp(); }}
};

struct LogTopic {
	std::vector<LogValue*> values;
	float rate;
	float updated = -INFINITY;

	template<typename... Names> LogTopic(float rate, const char *name, Names... names) : rate(rate) {
		const char *topicNames[] = {name, names...};
		for (const char *name : topicNames) {
			for (LogValue& value : logValues) {
				if (strcmp(value.name, name) == 0) {
					this->values.push_back(&value);
					break;
				}
			}
		}
	}
	template<typename... Names> LogTopic(const char *name, Names... names) : LogTopic(INFINITY, name, names...) {}
};

LogTopic logTopics[] = {
	{"t"},
	{"armed"},
	{"mode"},
	{1, "loopRate"},
	{"gyro.x", "gyro.y", "gyro.z"},
	{50, "acc.x", "acc.y", "acc.z"},
	{10, "gyroBias.x", "gyroBias.y", "gyroBias.z"},
	{50, "rates.x", "rates.y", "rates.z", "attitude.roll", "attitude.pitch", "attitude.yaw"},
	{10, "controlTime", "controlRoll", "controlPitch", "controlYaw", "controlThrottle"},
	{10, "ratesTarget.x", "ratesTarget.y", "ratesTarget.z", "attitudeTarget.roll", "attitudeTarget.pitch", "attitudeTarget.yaw", "thrustTarget"},
	{1, "motors[0]", "motors[1]", "motors[2]", "motors[3]"},
	{5, "voltage", "temp", "imuTemp"},
};

void *logBuffer; // buffer for log data
size_t logCapacity;
size_t logCursor = 0;
size_t logLength = 0;

void setupLog() {
	print("Setup log\n");

	free(logBuffer); // when reconfiguring
	logBuffer = nullptr;
	logCursor = 0;
	logLength = 0;

	if (logMemory == 0) {
		logCapacity = ESP.getFreeHeap() * logUsage;
		logBuffer = (uint8_t *)calloc(logCapacity, 1);
	} else if (logMemory == 1) {
		logCapacity = ESP.getFreePsram() * logUsage;
		logBuffer = (uint8_t *)heap_caps_calloc(logCapacity, 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	}
}

void loopLog() {
	if (logBuffer == nullptr || !armed) return;

	if (!logLength) resetLog(); // reset state on first log write

	static Rate sync(2);
	if (sync) {
		const uint8_t marker[] = {0x1A, 0x91, 0x4F, 0xF6, 0x7F};
		writeLog(&marker, sizeof(marker)); // write sync marker
	}

	for (uint8_t i = 0; i < ARRAY_SIZE(logTopics); i++) {
		LogTopic& topic = logTopics[i];
		if (t - topic.updated < 1 / topic.rate) continue; // throttle topic

		// Check if topic updated
		bool updated = false;
		for (LogValue *value : topic.values) {
			if (!floatEquals(value->cache, value->value.get())) {
				updated = true;
				break;
			}
		}
		if (!updated) continue;

		topic.updated = t;
		writeLog(&i, sizeof(i)); // write topic index

		for (LogValue *value : topic.values) {
			value->cache = value->value.get();
			writeLog(&value->cache, sizeof(value->cache)); // write topic value
		}
	}
}

void resetLog() {
	for (auto& topic : logTopics) {
		topic.updated = -INFINITY;
		for (LogValue *value : topic.values) {
			value->cache = NAN;
		}
	}
	logCursor = 0;
	logLength = 0;
}

void writeLog(const void *data, size_t size) {
	size_t first = min(size, logCapacity - logCursor);
	size_t second = size - first;
	memcpy(logBuffer + logCursor, data, first);
	logCursor = (logCursor + first) % logCapacity;
	if (second > 0) {
		memcpy(logBuffer + logCursor, data + first, second);
		logCursor = (logCursor + second) % logCapacity;
	}
	logLength = min(logLength + size, logCapacity);
}

void readLog(void *data, size_t position, size_t size) {
	if (logLength == logCapacity) {
		position = (logCursor + position) % logCapacity;
	}
	size_t first = min(size, logCapacity - position);
	size_t second = size - first;
	memcpy(data, logBuffer + position, first);
	if (second > 0) {
		memcpy(data + first, logBuffer, second);
	}
}

void printLogInfo() {
	if (logMemory == -1) return print("Log: disabled\n");

	print("Memory: %s\n", logMemory == 0 ? "RAM" : "PSRAM");
	print("Usage: %.f%%\n", logUsage * 100);
	print("Capacity: %u bytes\n", (unsigned)logCapacity);
	print("Used: %u bytes\n", (unsigned)logLength);
	print("Estimated duration: %d seconds\n", estimateLogDuration());
}

void printLogHeader() {
	for (int i = 0; i < ARRAY_SIZE(logTopics); i++) {
		print("Topic #%d (%g Hz):\n", i, logTopics[i].rate);
		for (LogValue *value : logTopics[i].values) {
			print("  %s%s\n", value->name, value->exposed ? " (exposed)" : "");
		}
	}
}

int estimateLogDuration() {
	float bandwidth = 0;
	for (LogTopic& topic : logTopics) {
		float rate = isinf(topic.rate) ? loopRate : topic.rate;
		bandwidth += rate * topic.values.size() * sizeof(float);
	}
	return logCapacity / bandwidth;
}

void printLogValues(const char *filter) {
	for (LogTopic& topic : logTopics) {
		for (LogValue *value : topic.values) {
			if (value->name == nullptr) break;
			if (strncasecmp(value->name, filter, strlen(filter))) continue;
			print("%s = %g\n", value->name, value->value.get());
		}
	}
}

void exposeLogValue(const char *name) {
	for (LogValue& value : logValues) {
		if (strcasecmp(value.name, name) == 0) {
			value.exposed = true;
			print("Log value %s exposed\n", name);
			return;
		}
	}
	print("Log value not found: %s\n", name);
}
