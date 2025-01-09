// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Motors output control using MOSFETs
// In case of using ESCs, change PWM_MIN and PWM_MAX definitions to appropriate values in μs

#define MOTOR_0_PIN 12 // rear left
#define MOTOR_1_PIN 13 // rear right
#define MOTOR_2_PIN 14 // front right
#define MOTOR_3_PIN 15 // front left

#define PWM_FREQUENCY 500
#define PWM_RESOLUTION 8
#define PWM_MIN 0
#define PWM_MAX 1000000 / PWM_FREQUENCY

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

uint8_t getDutyCycle(float value) {
	value = constrain(value, 0, 1);
	float pwm = mapff(value, 0, 1, PWM_MIN, PWM_MAX);
	float duty = mapff(pwm, 0, 1000000 / PWM_FREQUENCY, 0, (1 << PWM_RESOLUTION) - 1);
	return round(duty);
}

void sendMotors() {
	ledcWrite(MOTOR_0_PIN, getDutyCycle(motors[0]));
	ledcWrite(MOTOR_1_PIN, getDutyCycle(motors[1]));
	ledcWrite(MOTOR_2_PIN, getDutyCycle(motors[2]));
	ledcWrite(MOTOR_3_PIN, getDutyCycle(motors[3]));
}
