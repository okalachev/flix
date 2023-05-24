// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

#include "math.h"

float mapf(long x, long in_min, long in_max, float out_min, float out_max)
{
	return (float)(x - in_min) * (out_max - out_min) / (float)(in_max - in_min) + out_min;
}

float mapff(float x, float in_min, float in_max, float out_min, float out_max)
{
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

int8_t sign(float x)
{
	return (x > 0) - (x < 0);
}

float randomFloat(float min, float max)
{
	return min + (max - min) * (float)rand() / RAND_MAX;
}
