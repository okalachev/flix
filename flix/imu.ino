// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Work with the IMU sensor

#include <SPI.h>
#include <MPU9250.h>
#include "lpf.h"
#include "util.h"

MPU9250 IMU(SPI);

Vector accBias;
Vector accScale(1, 1, 1);
Vector gyroBias;

void setupIMU() {
	print("Setup IMU\n");
	IMU.begin();
	configureIMU();
}

void configureIMU() {
	IMU.setAccelRange(IMU.ACCEL_RANGE_4G);
	IMU.setGyroRange(IMU.GYRO_RANGE_2000DPS);
	IMU.setDLPF(IMU.DLPF_MAX);
	IMU.setRate(IMU.RATE_1KHZ_APPROX);
	IMU.setupInterrupt();
}

void readIMU() {
	IMU.waitForData();
	IMU.getGyro(gyro.x, gyro.y, gyro.z);
	IMU.getAccel(acc.x, acc.y, acc.z);
	calibrateGyroOnce();
	// apply scale and bias
	acc = (acc - accBias) / accScale;
	gyro = gyro - gyroBias;
	// rotate
	rotateIMU(acc);
	rotateIMU(gyro);
}

void rotateIMU(Vector& data) {
	// Rotate from LFD to FLU
	// NOTE: In case of using other IMU orientation, change this line:
	data = Vector(data.y, data.x, -data.z);
	// Axes orientation for various boards: https://github.com/okalachev/flixperiph#imu-axes-orientation
}

void calibrateGyroOnce() {
	static float landedTime = 0;
	landedTime = landed ? landedTime + dt : 0;
	if (landedTime < 2) return; // calibrate only if definitely stationary

	static LowPassFilter<Vector> gyroCalibrationFilter(0.001);
	gyroBias = gyroCalibrationFilter.update(gyro);
}

void calibrateAccel() {
	print("Calibrating accelerometer\n");
	IMU.setAccelRange(IMU.ACCEL_RANGE_2G); // the most sensitive mode

	print("1/6 Place level [8 sec]\n");
	pause(8);
	calibrateAccelOnce();
	print("2/6 Place nose up [8 sec]\n");
	pause(8);
	calibrateAccelOnce();
	print("3/6 Place nose down [8 sec]\n");
	pause(8);
	calibrateAccelOnce();
	print("4/6 Place on right side [8 sec]\n");
	pause(8);
	calibrateAccelOnce();
	print("5/6 Place on left side [8 sec]\n");
	pause(8);
	calibrateAccelOnce();
	print("6/6 Place upside down [8 sec]\n");
	pause(8);
	calibrateAccelOnce();

	printIMUCalibration();
	print("✓ Calibration done!\n");
	configureIMU();
}

void calibrateAccelOnce() {
	const int samples = 1000;
	static Vector accMax(-INFINITY, -INFINITY, -INFINITY);
	static Vector accMin(INFINITY, INFINITY, INFINITY);

	// Compute the average of the accelerometer readings
	acc = Vector(0, 0, 0);
	for (int i = 0; i < samples; i++) {
		IMU.waitForData();
		Vector sample;
		IMU.getAccel(sample.x, sample.y, sample.z);
		acc = acc + sample;
	}
	acc = acc / samples;

	// Update the maximum and minimum values
	if (acc.x > accMax.x) accMax.x = acc.x;
	if (acc.y > accMax.y) accMax.y = acc.y;
	if (acc.z > accMax.z) accMax.z = acc.z;
	if (acc.x < accMin.x) accMin.x = acc.x;
	if (acc.y < accMin.y) accMin.y = acc.y;
	if (acc.z < accMin.z) accMin.z = acc.z;
	// Compute scale and bias
	accScale = (accMax - accMin) / 2 / ONE_G;
	accBias = (accMax + accMin) / 2;
}

void printIMUCalibration() {
	print("gyro bias: %f %f %f\n", gyroBias.x, gyroBias.y, gyroBias.z);
	print("accel bias: %f %f %f\n", accBias.x, accBias.y, accBias.z);
	print("accel scale: %f %f %f\n", accScale.x, accScale.y, accScale.z);
}

void printIMUInfo() {
	IMU.status() ? print("status: ERROR %d\n", IMU.status()) : print("status: OK\n");
	print("model: %s\n", IMU.getModel());
	print("who am I: 0x%02X\n", IMU.whoAmI());
	print("rate: %.0f\n", loopRate);
	print("gyro: %f %f %f\n", rates.x, rates.y, rates.z);
	print("acc: %f %f %f\n", acc.x, acc.y, acc.z);
	Vector rawGyro, rawAcc;
	IMU.getGyro(rawGyro.x, rawGyro.y, rawGyro.z);
	IMU.getAccel(rawAcc.x, rawAcc.y, rawAcc.z);
	print("raw gyro: %f %f %f\n", rawGyro.x, rawGyro.y, rawGyro.z);
	print("raw acc: %f %f %f\n", rawAcc.x, rawAcc.y, rawAcc.z);
}
