// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Logging subsystem

#include "vector.h"
#include "util.h"

int logMemory = 0; // 0 - RAM, 1 - PSRAM, -1 - disabled
float logUsage = 0.5; // fraction of free memory to use for log

struct LogValue {
	const char *name;
	Value value;
	float lastValue = NAN;
	bool logged = true; // if false, use only for triggering log update
	LogValue() : name(nullptr), value() {}; // empty value constructor
	template <typename T>
	LogValue(const char *name, T value, bool logged = true) : name(name), value(value), logged(logged) {};
};

struct LogTopic {
	LogValue values[10];
	int length = 0; // number of logged values
	float throttle; // max update rate, Hz
	float lastUpdate = -INFINITY;

	LogTopic(float throttle, LogValue v0, LogValue v1 = {}, LogValue v2 = {}, LogValue v3 = {}, LogValue v4 = {}, LogValue v5 = {}, LogValue v6 = {}, LogValue v7 = {}, LogValue v8 = {}, LogValue v9 = {}) :
		throttle(throttle), values{v0, v1, v2, v3, v4, v5, v6, v7, v8, v9} {
			// Count logged values
			for (auto& v : values) {
				if (v.name == nullptr) break;
				if (v.logged) length++;
			}
		};

	LogTopic(LogValue v0, LogValue v1 = {}, LogValue v2 = {}, LogValue v3 = {}, LogValue v4 = {}, LogValue v5 = {}, LogValue v6 = {}, LogValue v7 = {}, LogValue v8 = {}, LogValue v9 = {}) :
		LogTopic(INFINITY, v0, v1, v2, v3, v4, v5, v6, v7, v8, v9) {};
};

LogTopic logTopics[] = {
	// time
	LogTopic({"t", &t}), // must be the first topic
	LogTopic(1, {"loopRate", &loopRate}),

	// imu
	LogTopic(
		{"gyro.x", &gyro.x},
		{"gyro.y", &gyro.y},
		{"gyro.z", &gyro.z}),

	LogTopic(50,
		{"acc.x", &acc.x},
		{"acc.y", &acc.y},
		{"acc.z", &acc.z}),

	LogTopic(10,
		{"gyroBias.x", &gyroBias.x},
		{"gyroBias.y", &gyroBias.y},
		{"gyroBias.z", &gyroBias.z}),

	// estimation
	LogTopic(50,
		{"rates.x", &rates.x},
		{"rates.y", &rates.y},
		{"rates.z", &rates.z},
		{"attitude.roll", []() { return attitude.getRoll(); }},
		{"attitude.pitch", []() { return attitude.getPitch(); }},
		{"attitude.yaw", []() { return attitude.getYaw(); }}),

	// rc
	LogTopic(10,
		{"controlTime", &controlTime, false}, // trigger value
		{"controlRoll", &controlRoll},
		{"controlPitch", &controlPitch},
		{"controlYaw", &controlYaw},
		{"controlThrottle", &controlThrottle}),

	// control
	LogTopic({"armed", &armed}),
	LogTopic({"mode", &mode}),

	LogTopic(10,
		{"ratesTarget.x", &ratesTarget.x},
		{"ratesTarget.y", &ratesTarget.y},
		{"ratesTarget.z", &ratesTarget.z},
		{"attitudeTarget.roll", []() { return attitudeTarget.getRoll(); }},
		{"attitudeTarget.pitch", []() { return attitudeTarget.getPitch(); }},
		{"attitudeTarget.yaw", []() { return attitudeTarget.getYaw(); }},
		{"thrustTarget", &thrustTarget}),

	// motors
	LogTopic(
		{"motors[0]", &motors[0]},
		{"motors[1]", &motors[1]},
		{"motors[2]", &motors[2]},
		{"motors[3]", &motors[3]}),

	// misc
	LogTopic(5,
		{"voltage", &voltage},
		{"temp", &temperatureRead},
		{"imuTemp", []() { return imu.getTemp(); }}),
};

void *logBuffer; // buffer for log data
size_t logCapacity;
size_t logCursor = 0;
size_t logLength = 0;
LogValue *logExposed = nullptr; // log values exposed to telemetry

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

	for (uint8_t i = 0; i < sizeof(logTopics) / sizeof(logTopics[0]); i++) {
		LogTopic& topic = logTopics[i];
		if (t - topic.lastUpdate < 1 / topic.throttle) continue; // throttle topic
		if (!isTopicUpdated(i)) continue; // skip if topic was't updated

		topic.lastUpdate = t;
		writeLog(&i, sizeof(i)); // write topic index

		for (auto& value : topic.values) {
			if (value.name == nullptr) break;
			if (!value.logged) continue;
			value.lastValue = value.value.get();
			writeLog(&value.lastValue, sizeof(float)); // write value
		}
	}
}

void resetLog() {
	for (auto& topic : logTopics) {
		topic.lastUpdate = -INFINITY;
		for (auto& value : topic.values) {
			value.lastValue = NAN;
		}
	}
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

bool isTopicUpdated(const uint8_t topic) {
	LogTopic& logTopic = logTopics[topic];
	bool updated = false;

	for (auto& value : logTopic.values) {
		if (value.name == nullptr) break;
		float v = value.value.get();
		if (!floatEquals(value.lastValue, v)) {
			value.lastValue = v;
			updated = true;
		}
	}
	return updated;
}

void printLogInfo() {
	if (logMemory == -1) return print("Log: disabled\n");

	print("Memory: %s\n", logMemory == 0 ? "RAM" : "PSRAM");
	print("Usage: %.f%%\n", logUsage * 100);
	print("Capacity: %u bytes\n", (unsigned)logCapacity);
	print("Used: %u bytes\n", (unsigned)logLength);
	print("Estimated duration: %d seconds\n", estimateLogDuration());
}

int estimateLogDuration() {
	float bandwidth = 0;
	for (LogTopic& topic : logTopics) {
		float rate = isinf(topic.throttle) ? loopRate : topic.throttle;
		bandwidth += rate * topic.length * sizeof(float);
	}
	return logCapacity / bandwidth;
}

void printLogHeader() {
	int i = 0;
	for (auto& topic : logTopics) {
		print("Topic #%d (%g Hz):\n", i++, topic.throttle);
		for (auto& value : topic.values) {
			if (value.name == nullptr) break;
			print("  %s%s\n", value.name, value.logged ?"" : " (not logged)");
		}
	}
}

void printLogValues(const char *filter) {
	for (LogTopic& topic : logTopics) {
		for (LogValue& value : topic.values) {
			if (value.name == nullptr) break;
			if (strncasecmp(value.name, filter, strlen(filter))) continue;
			print("%s = %g\n", value.name, value.value.get());
		}
	}
}

void configLogThrottle(const char *name, float throttle) {
	for (LogTopic& topic : logTopics) {
		for (LogValue& value : topic.values) {
			if (value.name == nullptr) break;
			if (strcasecmp(value.name, name) != 0) continue;
			topic.throttle = throttle;
			print("Log throttle for %s set to %.1f Hz\n", name, throttle);
			return;
		}
	}
	print("Log value not found: %s\n", name);
}

void exposeLogValue(const char *name) {
	for (int i = 0; i < sizeof(logTopics) / sizeof(logTopics[0]); i++) {
		LogTopic& topic = logTopics[i];
		for (LogValue& value : topic.values) {
			if (value.name == nullptr) break;
			if (strcasecmp(value.name, name) != 0) continue;
			logExposed = &value;
			print("Log value %s exposed\n", name);
			return;
		}
	}
	print("Log value not found: %s\n", name);
}
