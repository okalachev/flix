// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// https://habr.com/ru/company/first/blog/664922/
// esc: https://aliexpress.com/item/4000280617058.html (KINGDUO Micro Mini 4A 1S Brushed Esc 3.6-6V)
// motor: https://aliexpress.com/item/32731613504.html (8520 3.7V)

#define MOTOR_0_PIN 12
#define MOTOR_1_PIN 13
#define MOTOR_2_PIN 14
#define MOTOR_3_PIN 15

// #define PWM_FREQUENCY 200
// #define PWM_FREQUENCY 50 // TODO: way low frequency considering the IMU is 1kHz
#define PWM_FREQUENCY 200 // WARNING: original 50

#define PWM_RESOLUTION 8

// #define PWM_MIN 1575
// #define PWM_MAX 2300
#define PWM_NEUTRAL 1500
// #define PWM_REVERSE_MAX 700
// #define PWM_REVERSE_MIN 1425

// static const uint16_t pwmMin[] = {1600-50, 1600-50, 1600-50, 1600-50};
// static const uint16_t pwmMax[] = {2100, 2300, 2000, 2000}; // NOTE: ORIGINAL

static const uint16_t pwmMin[] = {1600, 1600, 1600, 1600}; // NOTE: success
// static const uint16_t pwmMax[] = {2000, 2000, 2000, 2000}; // NOTE: success
static const uint16_t pwmMax[] = {2300, 2300, 2300, 2300};
// from esc description
// static const uint16_t pwmMin[] = {1600, 1600, 1600, 1600};
// static const uint16_t pwmMax[] = {2300, 2300, 2300, 2300};

// static const uint16_t pwmReverseMin[] = {1420+50, 1440+50, 1440+50, 1440+50};

// static const uint16_t pwmReverseMin[] = {1420, 1440, 1440, 1440};
// static const uint16_t pwmReverseMax[] = {700, 1100, 1100, 1100}; // NOTE: ???
static const uint16_t pwmReverseMin[] = {1390, 1440, 1440, 1440};
static const uint16_t pwmReverseMax[] = {1100, 1100, 1100, 1100};

bool useBreak; // TODO: redesign

void setupMotors() {
	Serial.println("Setup Motors");

	// configure PWM channels
	ledcSetup(0, PWM_FREQUENCY, PWM_RESOLUTION);
	ledcSetup(1, PWM_FREQUENCY, PWM_RESOLUTION);
	ledcSetup(2, PWM_FREQUENCY, PWM_RESOLUTION);
	ledcSetup(3, PWM_FREQUENCY, PWM_RESOLUTION);

	// attach channels to motor pins
	ledcAttachPin(MOTOR_0_PIN, 0);
	ledcAttachPin(MOTOR_1_PIN, 1);
	ledcAttachPin(MOTOR_2_PIN, 2);
	ledcAttachPin(MOTOR_3_PIN, 3);

	// send initial break to initialize ESCs
	// Serial.println("Calibrating ESCs");
	// useBreak = true;
	// sendMotors();
	// delay(2000);
	// useBreak = false;
	sendMotors();
	Serial.println("Motors initialized");
}

static uint16_t getPWM(float val, int n)
{
	if (val == 0) {
		return PWM_NEUTRAL; // useBreak ? PWM_NEUTRAL : 0;
	} else if (val > 0) {
		return mapff(val, 0, 1, pwmMin[n], pwmMax[n]);
	} else {
		return mapff(val, 0, -1, pwmReverseMin[n], pwmReverseMax[n]);
	}
}

static uint8_t pwmToDutyCycle(uint16_t pwm) {
	return map(pwm, 0, 1000000 / PWM_FREQUENCY, 0, (1 << PWM_RESOLUTION) - 1);
}

void sendMotors()
{
	ledcWrite(0, pwmToDutyCycle(getPWM(motors[0], 0)));
	ledcWrite(1, pwmToDutyCycle(getPWM(motors[1], 1)));
	ledcWrite(2, pwmToDutyCycle(getPWM(motors[2], 2)));
	ledcWrite(3, pwmToDutyCycle(getPWM(motors[3], 3)));
}
