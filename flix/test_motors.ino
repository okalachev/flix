// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

static void _pwm(int n, uint16_t pwm)
{
	printf("Motor %d: %d\n", n, pwm);
	ledcWrite(n, pwmToDutyCycle(pwm));
	delay(5000);
}

void fullMotorTest(int n)
{
	printf("> Full test for motor %d\n", n);
	bool reverse = false;
	if (reverse) {
		// _pwm(n, 700);
		// _pwm(n, 800);
		// _pwm(n, 900);
		// _pwm(n, 1000);
		// _pwm(n, 1100);
		// _pwm(n, 1200);
		// _pwm(n, 1300);
		// _pwm(n, 1400);
		// _pwm(n, 1410);
		// _pwm(n, 1420);
		// _pwm(n, 1430);
		// _pwm(n, 1440);
		// _pwm(n, 1450);
		// _pwm(n, 1460);
		// _pwm(n, 1470);
		// _pwm(n, 1480);
		// _pwm(n, 1490);
	}
	_pwm(n, 1500);
	// _pwm(n, 1510);
	// _pwm(n, 1520);
	// _pwm(n, 1530);
	// _pwm(n, 1540);
	// _pwm(n, 1550);
	// _pwm(n, 1560);
	// _pwm(n, 1570);
	// _pwm(n, 1580);
	// _pwm(n, 1590);
	_pwm(n, 1600);
	_pwm(n, 1700);
	_pwm(n, 1800);
	_pwm(n, 1900);
	_pwm(n, 2000);
	_pwm(n, 2100);
	_pwm(n, 2200);
	_pwm(n, 2300);
	_pwm(n, 1500);
}

void fullMotorsTest()
{
	printf("Perform full motors test\n");
	motors[0] = 0;
	motors[1] = 0;
	motors[2] = 0;
	motors[3] = 0;

	fullMotorTest(0);
	fullMotorTest(1);
	fullMotorTest(2);
	fullMotorTest(3);
}
