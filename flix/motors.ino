// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Motors output control
// Motor: 8520 3.7V
// ESC: KINGDUO Micro Mini 4A 1S Brushed Esc 3.6-6V

#define MOTOR_0_PIN 12
#define MOTOR_1_PIN 13
#define MOTOR_2_PIN 14
#define MOTOR_3_PIN 15

#define PWM_FREQUENCY 200
#define PWM_RESOLUTION 8

#define PWM_NEUTRAL 1500

const uint16_t pwmMin[] = {1600, 1600, 1600, 1600};
const uint16_t pwmMax[] = {2300, 2300, 2300, 2300};
const uint16_t pwmReverseMin[] = {1390, 1440, 1440, 1440};
const uint16_t pwmReverseMax[] = {1100, 1100, 1100, 1100};

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

	sendMotors();
	Serial.println("Motors initialized");
}

uint16_t getPWM(float val, int n) {
	if (val == 0) {
		return PWM_NEUTRAL;
	} else if (val > 0) {
		return mapff(val, 0, 1, pwmMin[n], pwmMax[n]);
	} else {
		return mapff(val, 0, -1, pwmReverseMin[n], pwmReverseMax[n]);
	}
}

uint8_t pwmToDutyCycle(uint16_t pwm) {
	return map(pwm, 0, 1000000 / PWM_FREQUENCY, 0, (1 << PWM_RESOLUTION) - 1);
}

void sendMotors() {
	ledcWrite(0, pwmToDutyCycle(getPWM(motors[0], 0)));
	ledcWrite(1, pwmToDutyCycle(getPWM(motors[1], 1)));
	ledcWrite(2, pwmToDutyCycle(getPWM(motors[2], 2)));
	ledcWrite(3, pwmToDutyCycle(getPWM(motors[3], 3)));
}

void fullMotorTest(int n, bool reverse) {
	printf("Full test for motor %d\n", n);
	for (int pwm = PWM_NEUTRAL; pwm <= 2300 && pwm >= 700; pwm += reverse ? -100 : 100) {
		printf("Motor %d: %d\n", n, pwm);
		ledcWrite(n, pwmToDutyCycle(pwm));
		delay(3000);
	}
	printf("Motor %d: %d\n", n, PWM_NEUTRAL);
	ledcWrite(n, pwmToDutyCycle(PWM_NEUTRAL));
}
