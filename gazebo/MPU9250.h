// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix
// Distributed under the MIT License (https://opensource.org/licenses/MIT)

// Mocks of some MPU9250 library functions

#pragma once

class MPU9250 {
public:
	float getAccelBiasX_mss() { return 0; }
	float getAccelBiasY_mss() { return 0; }
	float getAccelBiasZ_mss() { return 0; }
	float getGyroBiasX_rads() { return 0; }
	float getGyroBiasY_rads() { return 0; }
	float getGyroBiasZ_rads() { return 0; }
};
