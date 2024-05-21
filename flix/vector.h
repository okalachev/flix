// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Lightweight vector library

#pragma once

class Vector : public Printable {
public:
	float x, y, z;

	Vector(): x(0), y(0), z(0) {};

	Vector(float x, float y, float z): x(x), y(y), z(z) {};

	float norm() const {
		return sqrt(x * x + y * y + z * z);
	}

	bool zero() const {
		return x == 0 && y == 0 && z == 0;
	}

	void normalize() {
		float n = norm();
		x /= n;
		y /= n;
		z /= n;
	}

	Vector operator * (const float b) const {
		return Vector(x * b, y * b, z * b);
	}

	Vector operator / (const float b) const {
		return Vector(x / b, y / b, z / b);
	}

	Vector operator + (const Vector& b) const {
		return Vector(x + b.x, y + b.y, z + b.z);
	}

	Vector operator - (const Vector& b) const {
		return Vector(x - b.x, y - b.y, z - b.z);
	}

	// Element-wise multiplication
	Vector operator * (const Vector& b) const {
		return Vector(x * b.x, y * b.y, z * b.z);
	}

	// Element-wise division
	Vector operator / (const Vector& b) const {
		return Vector(x / b.x, y / b.y, z / b.z);
	}

	inline bool operator == (const Vector& b) const {
		return x == b.x && y == b.y && z == b.z;
	}

	inline bool operator != (const Vector& b) const {
		return !(*this == b);
	}

	inline bool finite() const {
		return isfinite(x) && isfinite(y) && isfinite(z);
	}

	static float dot(const Vector& a, const Vector& b) {
		return a.x * b.x + a.y * b.y + a.z * b.z;
	}

	static Vector cross(const Vector& a, const Vector& b) {
		return Vector(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
	}

	static float angleBetweenVectors(const Vector& a, const Vector& b) {
		return acos(constrain(dot(a, b) / (a.norm() * b.norm()), -1, 1));
	}

	static Vector angularRatesBetweenVectors(const Vector& a, const Vector& b) {
		Vector direction = cross(a, b);
		if (direction.zero()) {
			// vectors are opposite, return any perpendicular vector
			return cross(a, Vector(1, 0, 0));
		}
		direction.normalize();
		float angle = angleBetweenVectors(a, b);
		return direction * angle;
	}

	size_t printTo(Print& p) const {
		return
			p.print(x, 15) + p.print(" ") +
			p.print(y, 15) + p.print(" ") +
			p.print(z, 15);
	}
};
