// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Joystick support for simulation

#include <SDL2/SDL.h>
#include <gazebo/gazebo.hh>
#include <iostream>

#define RC_CHANNEL_ROLL 0
#define RC_CHANNEL_PITCH 1
#define RC_CHANNEL_THROTTLE 2
#define RC_CHANNEL_YAW 3
#define RC_CHANNEL_ARMED 5
#define RC_CHANNEL_MODE 4

SDL_Joystick *joystick;
bool joystickInitialized = false, warnShown = false;

void joystickInit() {
	SDL_Init(SDL_INIT_JOYSTICK);
	joystick = SDL_JoystickOpen(0);
	if (joystick != NULL) {
		joystickInitialized = true;
		gzmsg << "Joystick initialized: " << SDL_JoystickNameForIndex(0) << std::endl;
	} else if (!warnShown) {
		gzwarn << "Joystick not found, begin waiting for joystick..." << std::endl;
		warnShown = true;
	}
}

bool joystickGet(int16_t ch[16]) {
	if (!joystickInitialized) {
		joystickInit();
		return false;
	}

	SDL_JoystickUpdate();

	for (uint8_t i = 0; i < sizeof(channels) / sizeof(channels[0]); i++) {
		ch[i] = SDL_JoystickGetAxis(joystick, i);
	}
	return true;
}
