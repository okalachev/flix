// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

#include <iostream>
#include <cmath>
#include <string>

using std::cout;
using std::max;
using std::min;
using std::isfinite;

// #define PI 3.1415926535897932384626433832795
#define DEG_TO_RAD 0.017453292519943295769236907684886
#define RAD_TO_DEG 57.295779513082320876798154814105

#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))

class Print;

class Printable {
public:
	virtual size_t printTo(Print& p) const = 0;
};

class Print {
public:
	size_t print(float n, int digits = 2)
	{
		cout << n;
		return 0; // TODO:
	}

	size_t println(float n, int digits = 2)
	{
		print(n, digits);
		cout << std::endl;
		return 0;
	}

	size_t print(const char* s)
	{
		cout << s;
		return 0;
	}

	size_t println(const char* s)
	{
		print(s);
		cout << std::endl;
		return 0;
	}

	size_t println(const Printable& p)
	{
		p.printTo(*this);
		cout << std::endl;
		return 0;
	}

	// int available()
	// {
	// 	std::string s;
	// 	s << std::cin;
	// 	return s.length();
	// }

	// char read()
	// {
	// 	char c;
	// 	s >> c;
	// 	return c;
	// }
};

class HardwareSerial: public Print {};

HardwareSerial Serial, Serial2;

// gazebo::common::Time curTime = this->dataPtr->model->GetWorld()->SimTime();

// unsigned long micros()
// {
// 	// return (unsigned long) (esp_timer_get_time());
// 	TODO:
// }
