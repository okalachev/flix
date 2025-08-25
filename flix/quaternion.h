// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Lightweight rotation quaternion library

#pragma once

#include "vector.h"

class Quaternion : public Printable {
public:
	float w, x, y, z;

	Quaternion(): w(1), x(0), y(0), z(0) {};

	Quaternion(float w, float x, float y, float z): w(w), x(x), y(y), z(z) {};

	static Quaternion fromAxisAngle(const Vector& axis, float angle) {
		float halfAngle = angle * 0.5;
		float sin2 = sin(halfAngle);
		float cos2 = cos(halfAngle);
		float sinNorm = sin2 / axis.norm();
		return Quaternion(cos2, axis.x * sinNorm, axis.y * sinNorm, axis.z * sinNorm);
	}

	static Quaternion fromRotationVector(const Vector& rotation) {
		if (rotation.zero()) {
			return Quaternion();
		}
		return Quaternion::fromAxisAngle(rotation, rotation.norm());
	}

	static Quaternion fromEuler(const Vector& euler) {
		float cx = cos(euler.x / 2);
		float cy = cos(euler.y / 2);
		float cz = cos(euler.z / 2);
		float sx = sin(euler.x / 2);
		float sy = sin(euler.y / 2);
		float sz = sin(euler.z / 2);

		return Quaternion(
			cx * cy * cz + sx * sy * sz,
			sx * cy * cz - cx * sy * sz,
			cx * sy * cz + sx * cy * sz,
			cx * cy * sz - sx * sy * cz);
	}

	static Quaternion fromBetweenVectors(Vector u, Vector v) {
		float dot = u.x * v.x + u.y * v.y + u.z * v.z;
		float w1 = u.y * v.z - u.z * v.y;
		float w2 = u.z * v.x - u.x * v.z;
		float w3 = u.x * v.y - u.y * v.x;

		Quaternion ret(
			dot + sqrt(dot * dot + w1 * w1 + w2 * w2 + w3 * w3),
			w1,
			w2,
			w3);
		ret.normalize();
		return ret;
	}

	bool finite() const {
		return isfinite(w) && isfinite(x) && isfinite(y) && isfinite(z);
	}

	bool valid() const {
		return finite();
	}

	bool invalid() const {
		return !valid();
	}

	void invalidate() {
		w = NAN;
		x = NAN;
		y = NAN;
		z = NAN;
	}


	float norm() const {
		return sqrt(w * w + x * x + y * y + z * z);
	}

	void normalize() {
		float n = norm();
		w /= n;
		x /= n;
		y /= n;
		z /= n;
	}

	void toAxisAngle(Vector& axis, float& angle) const {
		angle = acos(w) * 2;
		axis.x = x / sin(angle / 2);
		axis.y = y / sin(angle / 2);
		axis.z = z / sin(angle / 2);
	}

	Vector toRotationVector() const {
		if (w == 1 && x == 0 && y == 0 && z == 0) return Vector(0, 0, 0); // neutral quaternion
		float angle;
		Vector axis;
		toAxisAngle(axis, angle);
		return angle * axis;
	}

	Vector toEuler() const {
		// https://github.com/ros/geometry2/blob/589caf083cae9d8fae7effdb910454b4681b9ec1/tf2/include/tf2/impl/utils.h#L87
		Vector euler;
		float sqx = x * x;
		float sqy = y * y;
		float sqz = z * z;
		float sqw = w * w;
		// Cases derived from https://orbitalstation.wordpress.com/tag/quaternion/
		float sarg = -2 * (x * z - w * y) / (sqx + sqy + sqz + sqw);
		if (sarg <= -0.99999) {
			euler.x = 0;
			euler.y = -0.5 * PI;
			euler.z = -2 * atan2(y, x);
		} else if (sarg >= 0.99999) {
			euler.x = 0;
			euler.y = 0.5 * PI;
			euler.z = 2 * atan2(y, x);
		} else {
			euler.x = atan2(2 * (y * z + w * x), sqw - sqx - sqy + sqz);
			euler.y = asin(sarg);
			euler.z = atan2(2 * (x * y + w * z), sqw + sqx - sqy - sqz);
		}
		return euler;
	}

	float getRoll() const {
		return toEuler().x;
	}

	float getPitch() const {
		return toEuler().y;
	}

	float getYaw() const {
		return toEuler().z;
	}

	void setRoll(float roll) {
		Vector euler = toEuler();
		*this = Quaternion::fromEuler(Vector(roll, euler.y, euler.z));
	}

	void setPitch(float pitch) {
		Vector euler = toEuler();
		*this = Quaternion::fromEuler(Vector(euler.x, pitch, euler.z));
	}

	void setYaw(float yaw) {
		Vector euler = toEuler();
		*this = Quaternion::fromEuler(Vector(euler.x, euler.y, yaw));
	}

	Quaternion operator * (const Quaternion& q) const {
		return Quaternion(
			w * q.w - x * q.x - y * q.y - z * q.z,
			w * q.x + x * q.w + y * q.z - z * q.y,
			w * q.y + y * q.w + z * q.x - x * q.z,
			w * q.z + z * q.w + x * q.y - y * q.x);
	}

	bool operator == (const Quaternion& q) const {
		return w == q.w && x == q.x && y == q.y && z == q.z;
	}

	bool operator != (const Quaternion& q) const {
		return !(*this == q);
	}

	Quaternion inversed() const {
		float normSqInv = 1 / (w * w + x * x + y * y + z * z);
		return Quaternion(
			w * normSqInv,
			-x * normSqInv,
			-y * normSqInv,
			-z * normSqInv);
	}

	Vector conjugate(const Vector& v) const {
		Quaternion qv(0, v.x, v.y, v.z);
		Quaternion res = (*this) * qv * inversed();
		return Vector(res.x, res.y, res.z);
	}

	Vector conjugateInversed(const Vector& v) const {
		Quaternion qv(0, v.x, v.y, v.z);
		Quaternion res = inversed() * qv * (*this);
		return Vector(res.x, res.y, res.z);
	}

	// Rotate quaternion by quaternion
	static Quaternion rotate(const Quaternion& a, const Quaternion& b, const bool normalize = true) {
		Quaternion rotated = a * b;
		if (normalize) {
			rotated.normalize();
		}
		return rotated;
	}

	// Rotate vector by quaternion
	static Vector rotateVector(const Vector& v, const Quaternion& q) {
		return q.conjugateInversed(v);
	}

	// Quaternion between two quaternions a and b
	static Quaternion between(const Quaternion& a, const Quaternion& b, const bool normalize = true) {
		Quaternion q = a * b.inversed();
		if (normalize) {
			q.normalize();
		}
		return q;
	}

	size_t printTo(Print& p) const {
		size_t r = 0;
		r += p.print(w, 15) + p.print(" ");
		r += p.print(x, 15) + p.print(" ");
		r += p.print(y, 15) + p.print(" ");
		r += p.print(z, 15);
		return r;
	}
};
