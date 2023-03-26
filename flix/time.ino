// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

const uint32_t MS = 1000;
const uint32_t S = 1000000;

static uint32_t stepsPerSecondCurrent;
static uint32_t stepsPerSecondCurrentLast;

void setupTime()
{
	startTime = micros();
}

void step() {
	steps++;
	auto time = micros();

	if (stepTime == 0) { // first step
		stepTime = time;
	}

	dt = (time - stepTime) / 1000000.0;
	stepTime = time;

	// compute steps per seconds
	stepsPerSecondCurrent++;
	if (time - stepsPerSecondCurrentLast >= 1000000) {
		stepsPerSecond = stepsPerSecondCurrent;
		stepsPerSecondCurrent = 0;
		stepsPerSecondCurrentLast = time;
	}
}

void _step() {
	steps++;
	auto currentStepTime = micros();
	if (stepTime == 0) {
		stepTime = currentStepTime;
	}
	dt = (currentStepTime - stepTime) / 1000000.0;
	stepTime = currentStepTime;

	// compute steps per second, TODO: move to func
	stepsPerSecondCurrent++;
	if (stepTime - stepsPerSecondCurrentLast >= 1000000) {
		stepsPerSecond = stepsPerSecondCurrent;
		stepsPerSecondCurrent = 0;
		stepsPerSecondCurrentLast = stepTime;
	}
}
