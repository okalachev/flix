// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

#include <SDL2/SDL.h>
#include <gazebo/gazebo.hh>
#include <iostream>

using namespace std;

static const int16_t channelNeutralMin[] = {-1290,	-258,	-26833,	0,	0, 0};
static const int16_t channelNeutralMax[] = {-1032,	-258,	-27348,	3353,	0, 0};

static const int16_t channelMax[] =        {27090,	27090,	27090,	27090, 0, 0};

#define RC_CHANNEL_THROTTLE 2
#define RC_CHANNEL_YAW 3
#define RC_CHANNEL_PITCH 1
#define RC_CHANNEL_ROLL 0
#define RC_CHANNEL_AUX 4
#define RC_CHANNEL_MODE 5

static SDL_Joystick *joystick;

bool turnigyInitialized = false, warnShown = false;

void turnigyInit()
{
	SDL_Init(SDL_INIT_JOYSTICK);
	joystick = SDL_JoystickOpen(0);
	if (joystick != NULL) {
		turnigyInitialized = true;
		gzmsg << "Joystick initialized: " << SDL_JoystickNameForIndex(0) << endl;
	} else if (!warnShown) {
		gzwarn << "Joystick not found, begin waiting for joystick..." << endl;
		warnShown = true;
	}
}

void turnigyGet()
{
	if (!turnigyInitialized) {
		turnigyInit();
		return;
	}

	SDL_JoystickUpdate();

	for (uint8_t i = 0; i < 4; i++) {
		channels[i] = SDL_JoystickGetAxis(joystick, i);
	}

	normalizeRC();
}

void normalizeRC() {
	for (uint8_t i = 0; i < RC_CHANNELS; i++) {
		if (channels[i] >= channelNeutralMin[i] && channels[i] <= channelNeutralMax[i]) {
			controls[i] = 0; // make neutral position strictly equal to 0
		// } else if (channels[i] > channelNeutralMax[i]) {
		// 	controls[i] = mapf(channels[i], channelNeutralMax[i], channelMax[i], 0, 1);
		// } else {
		// 	float channelMin = channelNeutralMin[i] - (channelMax[i] - channelNeutralMax[i]);
		// 	controls[i] = mapf(channels[i], channelNeutralMin[i], channelMin, -1, 0);
		// }
		} else {
			controls[i] = mapf(channels[i], (channelNeutralMin[i] + channelNeutralMax[i]) / 2, channelMax[i], 0, 1);
		}
	}
	controls[RC_CHANNEL_THROTTLE] = constrain(controls[RC_CHANNEL_THROTTLE], 0, 1);
}
