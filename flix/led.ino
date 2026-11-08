// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// LED control

#include <algorithm>
#include <vector>
#include <FlixPeriph.h>
#include "util.h"

#ifndef LED_BUILTIN
#define LED_BUILTIN 2 // for ESP32 Dev Module
#endif

extern float voltageLow;

int ledType = 0; // 0 - single LED, 1 - WS2812, -1 - disabled
int ledPin = LED_BUILTIN;
int ledCount = 1;
float ledBrightness = 0.1;
float ledBrightnessArmed = 1;
bool ledShowMode = true;
int standbyEffect = RAINBOW;

WS2812 ws2812;
std::vector<RGB> led; // vector of RGB values

int effect = -1; // current effect
RGB effectColor;

const int FILL = 0, BLINK = 1, RAINBOW = 2;

void setupLED() {
	led.clear();

	if (ledType == 0) {
		pinMode(ledPin, OUTPUT);
		led.resize(1);
	} else if (ledType == 1) {
		led.resize(ledCount);
		ws2812.begin(ledPin, ledCount);
	}
}

void loopLED() {
	if (ledType == -1) return;

	static Rate rate(30);
	if (!rate) return;

	selectEffect();
	loopEffect();

	if (ledType == 0) {
		digitalWrite(ledPin, effectColor.r || effectColor.g || effectColor.b ? HIGH : LOW);
	} else if (ledType == 1) {
		ws2812.setBrightness(armed ? ledBrightnessArmed : ledBrightness);
		ws2812.write(led.data());
	}
}

void selectEffect() {
	if (effect == -1) startEffect(standbyEffect, 0, 0, 0);

	static int md = mode; // previous mode
	if (ledShowMode && ledType == 1 && mode != md) {
		if (mode == STAB) startEffect(FILL, 30, 180, 50);
		if (mode == ACRO) startEffect(FILL, 245, 155, 0);
		if (mode == RAW) startEffect(FILL, 255, 51, 50);
		if (mode == AUTO) startEffect(FILL, 220, 20, 250);
	}
	md = mode;

	if (voltage > 0 && voltage <= voltageLow) {
		startEffect(BLINK, 255, 0, 0); // blink red on low voltage
	}
}

void startEffect(int e, uint8_t r, uint8_t g, uint8_t b) {
	effect = e;
	effectColor = RGB(r, g, b);

	if (effect == FILL) {
		std::fill_n(led.begin(), led.size(), effectColor);
	}
}

void loopEffect() {
	if (effect == RAINBOW) {
		const float period = 10;
		float cyclePhase = fmod(t / period, 1.0f);
		for (int i = 0; i < ledCount; ++i) {
			float ledPhase = fmod(cyclePhase + (float)i / ledCount, 1.0f) * 3.0f;
			int segment = (int)ledPhase;
			uint8_t segmentLevel = (ledPhase - segment) * 255;
			led[i] = segment == 0 ? RGB(segmentLevel, (uint8_t)(255 - segmentLevel), 0)
				: segment == 1 ? RGB((uint8_t)(255 - segmentLevel), 0, segmentLevel)
				: RGB(0, segmentLevel, (uint8_t)(255 - segmentLevel));
		}
	}

	if (effect == BLINK) {
		const float period = 1;
		int state = fmod(t, period) < period / 2;
		for (int i = 0; i < ledCount; ++i) {
			led[i] = RGB(state ? effectColor.r : 0, state ? effectColor.g : 0, state ? effectColor.b : 0);
		}
	}
}
