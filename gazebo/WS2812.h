// Copyright (c) 2026 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// WS2812 library mock to make it possible to compile simulator

#pragma once

struct RGB {
	RGB() = default;
	RGB(uint8_t r, uint8_t g, uint8_t b) : r(r), g(g), b(b) {}
	uint8_t r, g, b;
};

class WS2812 {
public:
	bool begin(int, int) { return true; }
	void setBrightness(float) {}
	bool write(RGB*) { return true; }
};
