// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Motors output control
// Motor: 8520 3.7V
// ESC: KINGDUO Micro Mini 4A 1S Brushed Esc 3.6-6V

#define MOTOR_0_PIN 12 // rear left
#define MOTOR_1_PIN 13 // rear right
#define MOTOR_2_PIN 14 // front right
#define MOTOR_3_PIN 15 // front left

#define PWM_FREQUENCY 200
#define PWM_RESOLUTION 8
#define PWM_NEUTRAL 1500
#define PWM_MIN 1600
#define PWM_MAX 2300
#define PWM_REVERSE_MIN 1400
#define PWM_REVERSE_MAX 700

void setupMotors() {
	Serial.println("Setup Motors");

	// configure pins
	ledcAttach(MOTOR_0_PIN, PWM_FREQUENCY, PWM_RESOLUTION);
	ledcAttach(MOTOR_1_PIN, PWM_FREQUENCY, PWM_RESOLUTION);
	ledcAttach(MOTOR_2_PIN, PWM_FREQUENCY, PWM_RESOLUTION);
	ledcAttach(MOTOR_3_PIN, PWM_FREQUENCY, PWM_RESOLUTION);

	sendMotors();
	Serial.println("Motors initialized");
}

uint16_t getPWM(float val, int n) {
	if (val == 0) {
		return PWM_NEUTRAL;
	} else if (val > 0) {
		return mapff(val, 0, 1, PWM_MIN, PWM_MAX);
	} else {
		return mapff(val, 0, -1, PWM_REVERSE_MIN, PWM_REVERSE_MAX);
	}
}

uint8_t pwmToDutyCycle(uint16_t pwm) {
	return map(pwm, 0, 1000000 / PWM_FREQUENCY, 0, (1 << PWM_RESOLUTION) - 1);
}

void sendMotors() {
	ledcWrite(MOTOR_0_PIN, pwmToDutyCycle(getPWM(motors[0], 0)));
	ledcWrite(MOTOR_1_PIN, pwmToDutyCycle(getPWM(motors[1], 1)));
	ledcWrite(MOTOR_2_PIN, pwmToDutyCycle(getPWM(motors[2], 2)));
	ledcWrite(MOTOR_3_PIN, pwmToDutyCycle(getPWM(motors[3], 3)));
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
