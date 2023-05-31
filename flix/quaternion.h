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

	static Quaternion fromAxisAngle(float a, float b, float c, float angle)
	{
		float halfAngle = angle * 0.5;
		float sin2 = sin(halfAngle);
		float cos2 = cos(halfAngle);
		float sinNorm = sin2 / sqrt(a * a + b * b + c * c);
		return Quaternion(cos2, a * sinNorm, b * sinNorm, c * sinNorm);
	}

	static Quaternion fromAngularRates(float x, float y, float z)
	{
		return Quaternion::fromAxisAngle(x, y, z, sqrt(x * x + y * y + z * z));
	}

	static Quaternion fromAngularRates(const Vector& rates)
	{
		if (rates.zero()) {
			return Quaternion();
		}
		return Quaternion::fromAxisAngle(rates.x, rates.y, rates.z, rates.norm());
	}

	static Quaternion fromEulerZYX(float x, float y, float z)
	{
		float cx = cos(x / 2);
		float cy = cos(y / 2);
		float cz = cos(z / 2);
		float sx = sin(x / 2);
		float sy = sin(y / 2);
		float sz = sin(z / 2);

		return Quaternion(
			cx * cy * cz + sx * sy * sz,
			sx * cy * cz - cx * sy * sz,
			cx * sy * cz + sx * cy * sz,
			cx * cy * sz - sx * sy * cz);
	}

	static Quaternion fromBetweenVectors(Vector u, Vector v)
	{
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

	static Quaternion _fromBetweenVectors(float a, float b, float c, float x, float y, float z)
	{
		float dot = a * x + b * y + c * z;
		float w1 = b * z - c * y;
		float w2 = c * x - a * z;
		float w3 = a * y - b * x;

		Quaternion ret(
			dot + sqrt(dot * dot + w1 * w1 + w2 * w2 + w3 * w3),
			w1,
			w2,
			w3);
		ret.normalize();
		return ret;
	};

	void toAxisAngle(float& a, float& b, float& c, float& angle)
	{
		angle = acos(w) * 2;
		a = x / sin(angle / 2);
		b = y / sin(angle / 2);
		c = z / sin(angle / 2);
	}

	Vector toEulerZYX() const
	{
		return Vector(
			atan2(2 * (w * x + y * z), 1 - 2 * (x * x + y * y)),
			asin(2 * (w * y - z * x)),
			atan2(2 * (w * z + x * y), 1 - 2 * (y * y + z * z)));
	}

	float getYaw() const
	{
		// https://github.com/ros/geometry2/blob/589caf083cae9d8fae7effdb910454b4681b9ec1/tf2/include/tf2/impl/utils.h#L122
		float yaw;
		float sqx = x * x;
		float sqy = y * y;
		float sqz = z * z;
		float sqw = w * w;

		double sarg = -2 * (x * z - w * y) / (sqx + sqy + sqz + sqw);

		if (sarg <= -0.99999) {
			yaw = -2 * atan2(y, x);
		} else if (sarg >= 0.99999) {
			yaw = 2 * atan2(y, x);
		} else {
			yaw = atan2(2 * (x * y + w * z), sqw + sqx - sqy - sqz);
		}
		return yaw;
	}

	void setYaw(float yaw)
	{
		// TODO: optimize?
		Vector euler = toEulerZYX();
		(*this) = Quaternion::fromEulerZYX(euler.x, euler.y, yaw);
	}

	void toAngularRates(float& x, float& y, float& z)
	{
		// this->toAxisAngle(); // TODO:
	}

	Quaternion & operator*=(const Quaternion &q)
	{
		Quaternion ret(
			w * q.w - x * q.x - y * q.y - z * q.z,
			w * q.x + x * q.w + y * q.z - z * q.y,
			w * q.y + y * q.w + z * q.x - x * q.z,
			w * q.z + z * q.w + x * q.y - y * q.x);
		return (*this = ret);
	}

	Quaternion operator*(const Quaternion& q)
	{
		return Quaternion(
			w * q.w - x * q.x - y * q.y - z * q.z,
			w * q.x + x * q.w + y * q.z - z * q.y,
			w * q.y + y * q.w + z * q.x - x * q.z,
			w * q.z + z * q.w + x * q.y - y * q.x);
	}

	Quaternion inversed() const
	{
		float normSqInv = 1 / (w * w + x * x + y * y + z * z);
		return Quaternion(
			w * normSqInv,
			-x * normSqInv,
			-y * normSqInv,
			-z * normSqInv);
	}

	float norm() const
	{
		return sqrt(w * w + x * x + y * y + z * z);
	}

	void normalize()
	{
		float n = norm();
		w /= n;
		x /= n;
		y /= n;
		z /= n;
	}

	Vector conjugate(const Vector& v)
	{
		Quaternion qv(0, v.x, v.y, v.z);
		Quaternion res = (*this) * qv * inversed();
		return Vector(res.x, res.y, res.z);
	}

	Vector conjugateInversed(const Vector& v)
	{
		Quaternion qv(0, v.x, v.y, v.z);
		Quaternion res = inversed() * qv * (*this);
		return Vector(res.x, res.y, res.z);
	}

	inline Vector rotate(const Vector& v)
	{
		return conjugateInversed(v);
	}

	inline bool finite() const
	{
		return isfinite(w) && isfinite(x) && isfinite(y) && isfinite(z);
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