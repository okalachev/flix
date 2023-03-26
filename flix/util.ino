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

// float hypot3(float x, float y, float z)
// {
// 	return sqrt(x * x + y * y + z * z);
// }

int8_t sign(float x)
{
	return (x > 0) - (x < 0);
}

float randomFloat(float min, float max)
{
	return min + (max - min) * (float)rand() / RAND_MAX;
}

// === printf ===
// https://github.com/jandelgado/log4arduino/blob/master/src/log4arduino.cpp#L51
// https://webhamster.ru/mytetrashare/index/mtb0/16381244680p5beet5d6

#ifdef ARDUINO
#define PRINTF_MAX_STRING_LEN 200
void printf(const __FlashStringHelper *fmt, ...)
{
	char buf[PRINTF_MAX_STRING_LEN];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, PRINTF_MAX_STRING_LEN, (PGM_P)fmt, args);
	va_end(args);
	Serial.print(buf);
}
#endif
