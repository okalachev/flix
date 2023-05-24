// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// In-RAM logging

const int LOG_RATE = 100;
const int LOG_DURATION = 10;
const int LOG_PERIOD = 1000000 / LOG_RATE;
const int LOG_SIZE = LOG_DURATION * LOG_RATE;
const int LOG_COLUMNS = 14;

static float logBuffer[LOG_SIZE][LOG_COLUMNS]; // * 4 (float)
static int logPointer = 0;
static uint32_t lastLog = 0;

void logData()
{
	if (!armed) return;
	if (stepTime - lastLog < LOG_PERIOD) return;
	lastLog = stepTime;

	logBuffer[logPointer][0] = stepTime;
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

void dumpLog()
{
	Serial.printf("timestamp,rate.x,rate.y,rate.z,target.rate.x,target.rate.y,target.rate.z,"
		"attitude.x,attitude.y,attitude.z,target.attitude.x,target.attitude.y,target.attitude.z,thrust\n");
	for (int i = 0; i < LOG_SIZE; i++) {
		for (int j = 0; j < LOG_COLUMNS - 1; j++) {
			Serial.printf("%f,", logBuffer[i][j]);
		}
		Serial.printf("%f\n", logBuffer[i][LOG_COLUMNS - 1]);
	}
}
