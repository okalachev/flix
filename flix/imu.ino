// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

#include <SPI.h>
#include <MPU9250.h>

#define IMU_CS_PIN 4 // chip-select pin for IMU SPI connection

const bool CALIBRATE_GYRO_ON_START = false;

MPU9250 IMU(SPI, IMU_CS_PIN);

void setupIMU()
{
	Serial.println("Setup IMU");

	auto status = IMU.begin();
	if (status < 0) {
		while (true) {
			Serial.print("IMU begin error: "); Serial.println(status);
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
		Serial.println("IMU read error"); // TODO:
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

static void calibrateGyro()
{
	calibrating = true;
	Serial.println("Calibrating gyro, stand still");
	delay(500);
	int status = IMU.calibrateGyro();
	Serial.println("Calibration status: " + String(status));
	Serial.print("Gyro bias: ");
	Serial.print(IMU.getGyroBiasX_rads(), 10); Serial.print(" ");
	Serial.print(IMU.getGyroBiasY_rads(), 10); Serial.print(" ");
	Serial.println(IMU.getGyroBiasZ_rads(), 10);
	IMU.setSrd(0);
	calibrating = false;
}

static void calibrateAccel()
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
	Serial.print("Accel bias: ");
	Serial.print(IMU.getAccelBiasX_mss(), 10); Serial.print(" ");
	Serial.print(IMU.getAccelBiasY_mss(), 10); Serial.print(" ");
	Serial.println(IMU.getAccelBiasZ_mss(), 10);
	Serial.print("Accel scale: ");
	Serial.print(IMU.getAccelScaleFactorX(), 10); Serial.print(" ");
	Serial.print(IMU.getAccelScaleFactorY(), 10); Serial.print(" ");
	Serial.println(IMU.getAccelScaleFactorZ(), 10);
}

static void loadAccelCal()
{
	IMU.setAccelCalX(-0.0048542023, 1.0008112192);
	IMU.setAccelCalY(0.0521845818, 0.9985780716);
	IMU.setAccelCalZ(0.5754694939, 1.0045746565);
}

static void loadGyroCal()
{
	IMU.setGyroBiasX_rads(-0.0185128022);
	IMU.setGyroBiasY_rads(-0.0262369743);
	IMU.setGyroBiasZ_rads(0.0163032326);
}

// Accel bias: 0.0463809967 0.0463809967 0.1486964226
// Accel scale: 0.9986976385 0.9993721247 1.0561490059

// Accel bias: 0.0145006180 0.0145006180 0.0000000000
// Accel scale: 0.9989741445 0.9993283749 1.0000000000

// Correct:
// Accel bias: -0.0048542023 0.0521845818 0.5754694939
// Accel scale: 1.0008112192 0.9985780716 1.0045746565
