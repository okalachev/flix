// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// In-RAM logging

#define LOG_RATE 100
#define LOG_DURATION 10
#define LOG_PERIOD 1.0 / LOG_RATE
#define LOG_SIZE LOG_DURATION * LOG_RATE
#define LOG_COLUMNS 14

float logBuffer[LOG_SIZE][LOG_COLUMNS]; // * 4 (float)
int logPointer = 0;

void logData() {
	if (!armed) return;

	static float logTime = 0;
	if (t - logTime < LOG_PERIOD) return;
	logTime = t;

	logBuffer[logPointer][0] = t;
	logBuffer[logPointer][1] = rates.x;
	logBuffer[logPointer][2] = rates.y;
	logBuffer[logPointer][3] = rates.z;
	logBuffer[logPointer][4] = ratesTarget.x;
	logBuffer[logPointer][5] = ratesTarget.y;
	logBuffer[logPointer][6] = ratesTarget.z;
	logBuffer[logPointer][7] = attitude.toEulerZYX().x;
	logBuffer[logPointer][8] = attitude.toEulerZYX().y;
	logBuffer[logPointer][9] = attitude.toEulerZYX().z;
	logBuffer[logPointer][10] = attitudeTarget.toEulerZYX().x;
	logBuffer[logPointer][11] = attitudeTarget.toEulerZYX().y;
	logBuffer[logPointer][12] = attitudeTarget.toEulerZYX().z;
	logBuffer[logPointer][13] = thrustTarget;

	logPointer++;
	if (logPointer >= LOG_SIZE) {
		logPointer = 0;
	}
}

void dumpLog() {
	Serial.printf("t,rates.x,rates.y,rates.z,ratesTarget.x,ratesTarget.y,ratesTarget.z,"
		"attitude.x,attitude.y,attitude.z,attitudeTarget.x,attitudeTarget.y,attitudeTarget.z,thrustTarget\n");
	for (int i = 0; i < LOG_SIZE; i++) {
		if (logBuffer[i][0] == 0) continue; // skip empty records
		for (int j = 0; j < LOG_COLUMNS - 1; j++) {
			Serial.printf("%f,", logBuffer[i][j]);
		}
		Serial.printf("%f\n", logBuffer[i][LOG_COLUMNS - 1]);
	}
}
