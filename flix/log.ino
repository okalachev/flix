// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

#define LOG_RATE 200
#define LOG_DURATION 10
#define LOG_PERIOD 1000000 / LOG_RATE
#define LOG_SIZE LOG_DURATION * LOG_RATE
#define LOG_COLUMNS 11

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
	logBuffer[logPointer][7] = torqueTarget.x;
	logBuffer[logPointer][8] = torqueTarget.y;
	logBuffer[logPointer][9] = torqueTarget.z;
	logBuffer[logPointer][10] = thrustTarget;

	logPointer++;
	if (logPointer >= LOG_SIZE) {
		logPointer = 0;
	}
}

void dumpLog()
{
	printf("timestamp,rate.x,rate.y,rate.z,target.rate.x,target.rate.y,target.rate.z,torque.x,torque.y,torque.z,thrust\n");
	for (int i = 0; i < LOG_SIZE; i++) {
		for (int j = 0; j < LOG_COLUMNS - 1; j++) {
			printf("%f,", logBuffer[i][j]);
		}
		printf("%f\n", logBuffer[i][LOG_COLUMNS - 1]);
	}
	Serial.println();
}
