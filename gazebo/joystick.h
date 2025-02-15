// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Joystick support for simulation

#include <SDL2/SDL.h>
#include <gazebo/gazebo.hh>
#include <iostream>

SDL_Joystick *joystick;

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
	return joystickInitialized;
}

bool joystickGet(int16_t ch[16]) {
	SDL_JoystickUpdate();

	for (uint8_t i = 0; i < 16; i++) {
		ch[i] = SDL_JoystickGetAxis(joystick, i);
	}
	return true;
}
