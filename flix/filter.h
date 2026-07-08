// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Low pass and notch filters

#pragma once

template <typename T> // Using template to make the filter usable for scalar and vector values
class LowPassFilter {
public:
	float alpha; // smoothing constant, 1 means filter disabled
	T output;

	LowPassFilter(float alpha): alpha(alpha) {};

	T update(const T input) {
		if (!init) {
			init = true;
			return output = input;
		}
		return output += alpha * (input - output);
	}

	void setCutOffFrequency(float cutOffFreq, float dt) {
		alpha = 1 - exp(-2 * PI * cutOffFreq * dt);
	}

	void reset() {
		init = false;
	}

private:
	bool init = false;
};

template <typename T>
class NotchFilter {
public:
	float frequency;
	float bandwidth;
	T output;

	NotchFilter(float frequency, float bandwidth): frequency(frequency), bandwidth(bandwidth) {
		reset();
	};

	T update(const T input) {
		if (frequency <= 0 || bandwidth <= 0) return input;

		if (!init) {
			init = true;
			x1 = x2 = input;
			y1 = y2 = input;
			return output = input;
		}

		output = b0 * input + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;

		x2 = x1;
		x1 = input;
		y2 = y1;
		y1 = output;

		return output;
	}

	void reset() {
		const float dt = 0.001f;
		float f = frequency;
		float bw = bandwidth;
		if (f < 0) f = 0;
		if (bw < 1e-6f) bw = 1e-6f;

		float q = f / bw;
		if (q < 1e-3f) q = 1e-3f;

		const float w0 = 2.0f * PI * f * dt;
		const float c = cos(w0);
		const float s = sin(w0);
		const float alpha = s / (2.0f * q);

		const float a0 = 1.0f + alpha;
		const float invA0 = 1.0f / a0;

		b0 = 1.0f * invA0;
		b1 = -2.0f * c * invA0;
		b2 = 1.0f * invA0;
		a1 = -2.0f * c * invA0;
		a2 = (1.0f - alpha) * invA0;

		init = false;
	}

private:
	float b0, b1, b2, a1, a2;
	T x1, x2, y1, y2;
	bool init = false;
};
