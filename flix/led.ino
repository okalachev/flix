// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

#define LED_PIN 2
#define BLINK_FAST_PERIOD 300000
#define BLINK_SLOW_PERIOD 1000000

static bool state;

static enum {
	OFF,
	ON,
	BLINK_FAST,
	BLINK_SLOW
} LEDscheme = OFF;

void setupLED()
{
	pinMode(LED_BUILTIN, OUTPUT);
}

void setLED(bool on)
{
	digitalWrite(LED_BUILTIN, on ? HIGH : LOW);
}

void proceedLED()
{
	// TODO: this won't work
	// TODO:: just check is current second even or odd
	if (LEDscheme == BLINK_FAST && stepTime % BLINK_FAST_PERIOD == 0) {
		state != state;
		digitalWrite(LED_BUILTIN, state ? HIGH : LOW);
	} else if (LEDscheme == BLINK_SLOW && stepTime % BLINK_SLOW_PERIOD == 0) {
		state != state;
		digitalWrite(LED_BUILTIN, state ? HIGH : LOW);
	}
}

void blinkLED()
{
	setLED(micros() / 500000 % 2);
}
