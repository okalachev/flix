// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Work with the IMU sensor

#include <SPI.h>
#include <MPU9250.h>

#define IMU_CS_PIN 4 // chip-select pin for IMU SPI connection
#define CALIBRATE_GYRO_ON_START false

MPU9250 IMU(SPI, IMU_CS_PIN);

void setupIMU()
{
	Serial.println("Setup IMU");

	auto status = IMU.begin();
	if (status < 0) {
		while (true) {
			Serial.printf("IMU begin error: %d\n", status);
			delay(1000);
		}
	}

	if (CALIBRATE_GYRO_ON_START) {
		calibrateGyro();
	} else {
		loadGyroCal();
	}

	loadAccelCal();

	IMU.setSrd(0); // set sample rate to 1000 Hz
	// NOTE: very important, without the above the rate would be terrible 50 Hz
}

bool readIMU()
{
	if (IMU.readSensor() < 0) {
		Serial.println("IMU read error");
		return false;
	}

	auto lastRates = rates;

	rates.x = IMU.getGyroX_rads();
	rates.y = IMU.getGyroY_rads();
	rates.z = IMU.getGyroZ_rads();
	acc.x = IMU.getAccelX_mss();
	acc.y = IMU.getAccelY_mss();
	acc.z = IMU.getAccelZ_mss();

	return rates != lastRates;
}

void calibrateGyro()
{
	Serial.println("Calibrating gyro, stand still");
	delay(500);
	int status = IMU.calibrateGyro();
	Serial.printf("Calibration status: %d\n", status);
	IMU.setSrd(0);
	printIMUCal();
}

void calibrateAccel()
{
	Serial.println("Cal accel: place level"); delay(3000);
	IMU.calibrateAccel();
	Serial.println("Cal accel: place nose up"); delay(3000);
	IMU.calibrateAccel();
	Serial.println("Cal accel: place nose down"); delay(3000);
	IMU.calibrateAccel();
	Serial.println("Cal accel: place on right side"); delay(3000);
	IMU.calibrateAccel();
	Serial.println("Cal accel: place on left side"); delay(3000);
	IMU.calibrateAccel();
	Serial.println("Cal accel: upside down"); delay(300);
	IMU.calibrateAccel();
	printIMUCal();
}

void loadAccelCal()
{
	// NOTE: this should be changed to the actual values
	IMU.setAccelCalX(-0.0048542023, 1.0008112192);
	IMU.setAccelCalY(0.0521845818, 0.9985780716);
	IMU.setAccelCalZ(0.5754694939, 1.0045746565);
}

void loadGyroCal()
{
	// NOTE: this should be changed to the actual values
	IMU.setGyroBiasX_rads(-0.0185128022);
	IMU.setGyroBiasY_rads(-0.0262369743);
	IMU.setGyroBiasZ_rads(0.0163032326);
}

void printIMUCal()
{
	Serial.printf("gyro bias: %f %f %f\n", IMU.getGyroBiasX_rads(), IMU.getGyroBiasY_rads(), IMU.getGyroBiasZ_rads());
	Serial.printf("accel bias: %f %f %f\n", IMU.getAccelBiasX_mss(), IMU.getAccelBiasY_mss(), IMU.getAccelBiasZ_mss());
	Serial.printf("accel scale: %f %f %f\n", IMU.getAccelScaleFactorX(), IMU.getAccelScaleFactorY(), IMU.getAccelScaleFactorZ());
}
