// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Partial implementation of Arduino API for simulation

#pragma once

#include <cmath>
#include <string>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/poll.h>
#include <chrono>
#include <thread>

#define PI 3.1415926535897932384626433832795
#define DEG_TO_RAD 0.017453292519943295769236907684886
#define RAD_TO_DEG 57.295779513082320876798154814105
#define radians(deg) ((deg)*DEG_TO_RAD)
#define degrees(rad) ((rad)*RAD_TO_DEG)

#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))

long map(long x, long in_min, long in_max, long out_min, long out_max) {
	const long run = in_max - in_min;
	const long rise = out_max - out_min;
	const long delta = x - in_min;
	return (delta * rise) / run + out_min;
}

size_t strlcpy(char* dst, const char* src, size_t len) {
	size_t l = strlen(src);
	size_t i = 0;
	while (i < len - 1 && *src != '\0') { *dst++ = *src++; i++; }
	*dst = '\0';
	return l;
}

class __FlashStringHelper;

// Arduino String partial implementation
// https://www.arduino.cc/reference/en/language/variables/data-types/stringobject/
class String: public std::string {
public:
	String(const char *str = "") : std::string(str ? str : "") {}
	long toInt() const { return atol(this->c_str()); }
	float toFloat() const { return atof(this->c_str()); }
	bool isEmpty() const { return this->empty(); }
	void toCharArray(char *buf, unsigned int bufsize, unsigned int index = 0) const {
		strlcpy(buf, this->c_str() + index, bufsize);
	}
	void trim() {
		this->erase(0, this->find_first_not_of(" \t\n\r"));
		this->erase(this->find_last_not_of(" \t\n\r") + 1);
	}
	void toLowerCase() {
		std::transform(this->begin(), this->end(), this->begin(),
			[](unsigned char c) { return std::tolower(c); });
	}
};

class Print;

class Printable {
public:
	virtual size_t printTo(Print& p) const = 0;
};

class Print {
public:
	size_t printf(const char *format, ...) {
		va_list args;
		va_start(args, format);
		size_t result = vprintf(format, args);
		va_end(args);
		return result;
	}

	size_t print(int n) {
		return printf("%d", n);
	}

	size_t print(float n, int digits = 2) {
		return printf("%.*f", digits, n);
	}

	size_t println(float n, int digits = 2) {
		return printf("%.*f\n", digits, n);
	}

	size_t print(const char* s) {
		return printf("%s", s);
	}

	size_t println() {
		return print("\n");
	}

	size_t println(const char* s) {
		return printf("%s\n", s);
	}

	size_t println(const Printable& p) {
		return p.printTo(*this) + print("\n");
	}

	size_t print(const String& s) {
		return printf("%s", s.c_str());
	}

	size_t println(const std::string& s) {
		return printf("%s\n", s.c_str());
	}

	size_t println(const String& s) {
		return printf("%s\n", s.c_str());
	}
};

class HardwareSerial: public Print {
public:
	void begin(unsigned long baud) {
		// server is running in background by default, so it doesn't have access to stdin
		// https://github.com/gazebosim/gazebo-classic/blob/d45feeb51f773e63960616880b0544770b8d1ad7/gazebo/gazebo_main.cc#L216
		// set foreground process group to current process group to allow reading from stdin
		// https://stackoverflow.com/questions/58918188/why-is-stdin-not-propagated-to-child-process-of-different-process-group
		signal(SIGTTOU, SIG_IGN);
		tcsetpgrp(STDIN_FILENO, getpgrp());
		signal(SIGTTOU, SIG_DFL);
	};

	int available() {
		// to implement for Windows, see https://stackoverflow.com/a/71992965/6850197
		struct pollfd pfd = { .fd = STDIN_FILENO, .events = POLLIN };
		return poll(&pfd, 1, 0) > 0;
	}

	int read() {
		if (available()) {
			char c;
			size_t res = ::read(STDIN_FILENO, &c, 1); // use raw read to avoid C++ buffering
			// https://stackoverflow.com/questions/45238997/does-getchar-function-has-its-own-buffer-to-store-remaining-input
			return c;
		}
		return -1;
	}

	void setRxInvert(bool invert) {};
};

HardwareSerial Serial, Serial2;

class EspClass {
public:
	void restart() { Serial.println("Ignore reboot in simulation"); }
} ESP;

unsigned long __delayTime = 0;

void delay(uint32_t ms) {
	std::this_thread::sleep_for(std::chrono::milliseconds(ms));
	__delayTime += ms * 1000;
}

bool ledcAttach(uint8_t pin, uint32_t freq, uint8_t resolution) { return true; }
bool ledcWrite(uint8_t pin, uint32_t duty) { return true; }

unsigned long __micros;
unsigned long __resetTime = 0;

unsigned long micros() {
	return __micros + __resetTime + __delayTime; // keep the time monotonic
}
