// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Joystick support for simulation

#include <SDL2/SDL.h>
#include <gazebo/gazebo.hh>
#include <iostream>

// simulation calibration overrides, NOTE: use `cr` command and replace with the actual values
const int channelZeroOverride[] = {1500, 0, 1000, 1500, 1500, 1000};
const int channelMaxOverride[] = {2000, 2000, 2000, 2000, 2000, 2000};

// channels mapping overrides
const int rollChannelOverride = 3;
const int pitchChannelOverride = 4;
const int throttleChannelOverride = 5;
const int yawChannelOverride = 0;
const int armedChannelOverride = 2;
const int modeChannelOverride = 1;

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

	// apply overrides
	extern int channelZero[16];
	extern int channelMax[16];
	memcpy(channelZero, channelZeroOverride, sizeof(channelZeroOverride));
	memcpy(channelMax, channelMaxOverride, sizeof(channelMaxOverride));

	extern int rollChannel, pitchChannel, throttleChannel, yawChannel, armedChannel, modeChannel;
	rollChannel = rollChannelOverride;
	pitchChannel = pitchChannelOverride;
	throttleChannel = throttleChannelOverride;
	yawChannel = yawChannelOverride;
	armedChannel = armedChannelOverride;
	modeChannel = modeChannelOverride;

	return joystickInitialized;
}

bool joystickGet(int16_t ch[16]) {
	SDL_JoystickUpdate();

	for (uint8_t i = 0; i < 16; i++) {
		ch[i] = SDL_JoystickGetAxis(joystick, i);
	}
	return true;
}
