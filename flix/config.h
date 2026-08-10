// Copyright (c) 2026 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Default configuration parameters

#pragma once

// IMU
#define IMU_MODEL -1
#define IMU_BUS 0
#define IMU_PIN_INT -1
#define IMU_ROT_ROLL 0
#define IMU_ROT_PITCH 0
#define IMU_ROT_YAW -PI/2

// Motors
#define MOTOR_PIN_FL 12
#define MOTOR_PIN_FR 13
#define MOTOR_PIN_RL 14
#define MOTOR_PIN_RR 15
#define MOT_PWM_FREQ 78000
#define MOT_PWM_RES 10
#define MOT_PWM_STOP 0
#define MOT_PWM_MIN 0
#define MOT_PWM_MAX -1

// Control
#define PITCHRATE_P 0.05
#define PITCHRATE_I 0.2
#define PITCHRATE_D 0.001
#define PITCHRATE_I_LIM 0.3
#define ROLLRATE_P PITCHRATE_P
#define ROLLRATE_I PITCHRATE_I
#define ROLLRATE_D PITCHRATE_D
#define ROLLRATE_I_LIM PITCHRATE_I_LIM
#define YAWRATE_P 0.3
#define YAWRATE_I 0.0
#define YAWRATE_D 0.0
#define YAWRATE_I_LIM 0.3
#define ROLL_P 6
#define ROLL_I 0
#define ROLL_D 0
#define PITCH_P ROLL_P
#define PITCH_I ROLL_I
#define PITCH_D ROLL_D
#define YAW_P 3
#define RATES_D_LPF_ALPHA 0.2 // cutoff frequency ~ 40 Hz

// Power
#define VOLTAGE_PIN -1
#define VOLTAGE_SCALE 2

// RC
#define RC_PIN_RX -1
