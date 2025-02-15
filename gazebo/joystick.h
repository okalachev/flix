// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Joystick support for simulation

#include <SDL2/SDL.h>
#include <gazebo/gazebo.hh>
#include <iostream>

// simulation calibration overrides, NOTE: use `cr` command and replace with the actual values
const int channelNeutralOverride[] = {-258, -258, -27349, 0, -27349, 0};
const int channelMaxOverride[] = {27090, 27090, 27090, 27090, -5676, 1};

#define RC_CHANNEL_ROLL 0
#define RC_CHANNEL_PITCH 1
#define RC_CHANNEL_THROTTLE 2
#define RC_CHANNEL_YAW 3
#define RC_CHANNEL_ARMED 5
#define RC_CHANNEL_MODE 4

SDL_Joystick *joystick;

void normalizeRC();

bool joystickInit() {
	static bool joystickInitialized = false;
	static bool warnShown = false;
	if (joystickInitialized) return true;

	SDL_Init(SDL_INIT_JOYSTICK);
	joystick = SDL_JoystickOpen(0);
	if (joystick != NULL) {
		joystickInitialized = true;
		gzmsg << "Joystick initialized: " << SDL_JoystickNameForIndex(0) << std::endl;
	} else if (!warnShown) {
		gzwarn << "Joystick not found, begin waiting for joystick..." << std::endl;
		warnShown = true;
	}

	// apply calibration overrides
	extern int channelNeutral[16];
	extern int channelMax[16];
	memcpy(channelNeutral, channelNeutralOverride, sizeof(channelNeutralOverride));
	memcpy(channelMax, channelMaxOverride, sizeof(channelMaxOverride));

	return joystickInitialized;
}

bool joystickGet(int16_t ch[16]) {
	SDL_JoystickUpdate();

	for (uint8_t i = 0; i < 16; i++) {
		ch[i] = SDL_JoystickGetAxis(joystick, i);
	}
	return true;
}
