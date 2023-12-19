// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Joystick support for simulation

#include <SDL2/SDL.h>
#include <gazebo/gazebo.hh>
#include <iostream>

using namespace std;

static const int16_t channelNeutralMin[] = {-1290,	-258,	-26833,	0,	0, 0};
static const int16_t channelNeutralMax[] = {-1032,	-258,	-27348,	3353,	0, 0};

static const int16_t channelMax[] =        {27090,	27090,	27090,	27090, 0, 0};

#define RC_CHANNEL_ROLL 0
#define RC_CHANNEL_PITCH 1
#define RC_CHANNEL_THROTTLE 2
#define RC_CHANNEL_YAW 3
#define RC_CHANNEL_AUX 4
#define RC_CHANNEL_MODE 5

static SDL_Joystick *joystick;

bool joystickInitialized = false, warnShown = false;

void normalizeRC();

void joystickInit()
{
	SDL_Init(SDL_INIT_JOYSTICK);
	joystick = SDL_JoystickOpen(0);
	if (joystick != NULL) {
		joystickInitialized = true;
		gzmsg << "Joystick initialized: " << SDL_JoystickNameForIndex(0) << endl;
	} else if (!warnShown) {
		gzwarn << "Joystick not found, begin waiting for joystick..." << endl;
		warnShown = true;
	}
}

void joystickGet()
{
	if (!joystickInitialized) {
		joystickInit();
		return;
	}

	SDL_JoystickUpdate();

	for (uint8_t i = 0; i < 4; i++) {
		channels[i] = SDL_JoystickGetAxis(joystick, i);
	}
	channels[RC_CHANNEL_MODE] = SDL_JoystickGetButton(joystick, 0) ? 1 : 0;
	controls[RC_CHANNEL_MODE] = channels[RC_CHANNEL_MODE];

	normalizeRC();
}

void normalizeRC() {
	for (uint8_t i = 0; i < 4; i++) {
		if (channels[i] >= channelNeutralMin[i] && channels[i] <= channelNeutralMax[i]) {
			controls[i] = 0;
		} else {
			controls[i] = mapf(channels[i], (channelNeutralMin[i] + channelNeutralMax[i]) / 2, channelMax[i], 0, 1);
		}
	}
	controls[RC_CHANNEL_THROTTLE] = constrain(controls[RC_CHANNEL_THROTTLE], 0, 1);
}
