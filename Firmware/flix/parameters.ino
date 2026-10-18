// Copyright (c) 2024 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Parameters storage in flash memory

/*
 * ============================================================================
 * 【耀坤整理版】参数表与参数持久化（NVS / Preferences）
 * ----------------------------------------------------------------------------
 * 设计要点：
 *   - parameters[] 是本固件**唯一**的「可调参数注册表」。控制台 `p` 命令、地面站
 *     PARAM_REQUEST_LIST / PARAM_SET、以及开机 setDefaults() 全部围绕这张表工作。
 *   - 每个参数项 = {名称, 变量指针, 变更回调}。变量指针指向真正的全局变量，
 *     所以 `p CTL_RATE_R_P 0.2` 写入后立刻生效，不需要额外的同步代码。
 *   - 名称最长 15 字符（MAVLink PARAM_VALUE.param_id 只有 16 字节，含结尾 0）。
 *   - 带回调的参数（如 MOT_PIN_* → setupMotors）在值改变后会重新初始化对应外设。
 *
 * 【R10】本版相较上游新增的参数：
 *   IMU_AUTO_DETECT  开 / 关 IMU 自动识别（1=开，出厂默认）
 *   IMU_ROT_LOCK     锁定当前 IMU 朝向，禁止自动识别再改（1=锁，校准后置 1）
 *   LED_R_PIN / LED_G_PIN / LED_B_PIN   三色指示灯 GPIO（耀坤 S3 板：41/7/14）
 *   LED_LOW_VOLT     低电告警阈值（伏）
 *   BUZ_PIN          蜂鸣器 GPIO（耀坤 S3 板：39）
 *   BUZ_ENABLE       蜂鸣器总开关（0 = 静音，夜间调试用）
 *
 * 注：开机指纹串 identityString 定义在 flix.ino，**不放进参数表** —— 它是字符串，
 *    而参数表的存储是 float/int 联合体，塞字符串需要额外机制。指纹只在开机横幅输出。
 *
 * ⚠️ 关于 NVS 的坑：参数值存在 Preferences("flix") 命名空间里。烧写「带 NVS 状态」
 *    的固件时，如果整片从 0x0 连写会覆盖 0x9000 的 NVS 分区，导致 AP SSID、IMU
 *    识别结果、遥控标定值全部丢失。必须按 build/<fqbn>/flash_args 的分段偏移烧写。
 * ============================================================================
 */

#include <Preferences.h>
#include "util.h"
#include "i18n.h"   // 界面文字唯一数据源：中/英文可切（FLIX_LANG_EN）

extern int channelZero[16], channelMax[16];
extern int rollChannel, pitchChannel, throttleChannel, yawChannel, armedChannel, modeChannel;
extern int rcRxPin, voltagePin;
extern int wifiMode, wifiLongRange, wifiBroadcast, udpLocalPort, udpRemotePort, espnowChannel;
extern float rcLossTimeout, descendTime, disarmTilt;
extern float voltageScale;
extern LowPassFilter<float> voltageFilter;

// 耀坤整理版新增的外部变量（定义在 imu.ino / led.ino）
extern int imuAutoDetect, imuRotLock;
extern int ledRedPin, ledGreenPin, ledBluePin;
extern float ledLowVoltage;
extern int buzzerPin, buzzerEnable;

#include "config.h"

Preferences storage;

struct Parameter {
	const char *name; // max length is 15
	bool integer;
	union { float *f; int *i; }; // pointer to the variable
	float initial; // default value
	float cache; // what's stored in flash
	void (*callback)(); // called after parameter change
	Parameter(const char *name, float *variable, void (*callback)() = nullptr) : name(name), integer(false), f(variable), callback(callback) {};
	Parameter(const char *name, int *variable, void (*callback)() = nullptr) : name(name), integer(true), i(variable), callback(callback) {};
	float getValue() const { return integer ? *i : *f; };
	void setValue(const float value) { if (integer) *i = value; else *f = value; };
};

Parameter parameters[] = {
	// control
	{"CTL_RATE_R_P", &rollRatePID.p},
	{"CTL_RATE_R_I", &rollRatePID.i},
	{"CTL_RATE_R_D", &rollRatePID.d},
	{"CTL_RATE_R_WU", &rollRatePID.windup},
	{"CTL_RATE_R_D_A", &rollRatePID.lpf.alpha},
	{"CTL_RATE_P_P", &pitchRatePID.p},
	{"CTL_RATE_P_I", &pitchRatePID.i},
	{"CTL_RATE_P_D", &pitchRatePID.d},
	{"CTL_RATE_P_WU", &pitchRatePID.windup},
	{"CTL_RATE_P_D_A", &pitchRatePID.lpf.alpha},
	{"CTL_RATE_Y_P", &yawRatePID.p},
	{"CTL_RATE_Y_I", &yawRatePID.i},
	{"CTL_RATE_Y_D", &yawRatePID.d},
	{"CTL_RATE_Y_WU", &yawRatePID.windup},
	{"CTL_RATE_Y_D_A", &yawRatePID.lpf.alpha},
	{"CTL_RATE_P_MAX", &maxRate.y},
	{"CTL_RATE_R_MAX", &maxRate.x},
	{"CTL_RATE_Y_MAX", &maxRate.z},
	{"CTL_ATT_R_P", &rollPID.p},
	{"CTL_ATT_P_P", &pitchPID.p},
	{"CTL_ATT_Y_P", &yawPID.p},
	{"CTL_ATT_MAX", &tiltMax},
	{"CTL_FLT_MODE_0", &flightModes[0]},
	{"CTL_FLT_MODE_1", &flightModes[1]},
	{"CTL_FLT_MODE_2", &flightModes[2]},
	// imu
	{"IMU_MODEL", &imuModel},
	{"IMU_BUS", &imuBus},
	{"IMU_PIN_SCK", &imuSckPin},
	{"IMU_PIN_MISO", &imuMisoPin},
	{"IMU_PIN_MOSI", &imuMosiPin},
	{"IMU_PIN_CS", &imuCsPin},
	{"IMU_PIN_SDA", &imuSdaPin},
	{"IMU_PIN_SCL", &imuSclPin},
	{"IMU_PIN_INT", &imuIntPin},
	{"IMU_ROT_ROLL", &imuRotation.x},
	{"IMU_ROT_PITCH", &imuRotation.y},
	{"IMU_ROT_YAW", &imuRotation.z},
	{"IMU_ACC_BIAS_X", &accBias.x},
	{"IMU_ACC_BIAS_Y", &accBias.y},
	{"IMU_ACC_BIAS_Z", &accBias.z},
	{"IMU_ACC_SCALE_X", &accScale.x},
	{"IMU_ACC_SCALE_Y", &accScale.y},
	{"IMU_ACC_SCALE_Z", &accScale.z},
	{"IMU_GYRO_BIAS_A", &gyroBiasFilter.alpha},
	// 【R10 新增】IMU 自动识别 / 朝向锁定
	// IMU_AUTO_DETECT=1：开机裸读 WHO_AM_I 判断是 MPU6050(I2C) 还是 MPU6500(SPI)，
	//   并自动套用对应朝向。换 IMU 不用改代码重编，直接上电即可。
	// IMU_ROT_LOCK=1：锁定朝向，自动识别不再覆盖。跑完 `cal`（IMU 校准）后应置 1，
	//   避免下次上电探测失败时把已标定好的朝向改错。
	{"IMU_AUTO_DETECT", &imuAutoDetect},
	{"IMU_ROT_LOCK", &imuRotLock},
	// estimate
	{"EST_ACC_WEIGHT", &accWeight},
	{"EST_LVL_WEIGHT", &levelWeight},
	{"EST_RATES_LPF_A", &ratesFilter.alpha},
	// motors
	{"MOT_PIN_FL", &motorPins[MOT_FL], setupMotors},
	{"MOT_PIN_FR", &motorPins[MOT_FR], setupMotors},
	{"MOT_PIN_RL", &motorPins[MOT_RL], setupMotors},
	{"MOT_PIN_RR", &motorPins[MOT_RR], setupMotors},
	{"MOT_PWM_FREQ", &pwmFrequency, setupMotors},
	{"MOT_PWM_RES", &pwmResolution, setupMotors},
	{"MOT_PWM_STOP", &pwmStop},
	{"MOT_PWM_MIN", &pwmMin},
	{"MOT_PWM_MAX", &pwmMax},
	// rc
	{"RC_RX_PIN", &rcRxPin, setupRC},
	{"RC_ZERO_0", &channelZero[0]},
	{"RC_ZERO_1", &channelZero[1]},
	{"RC_ZERO_2", &channelZero[2]},
	{"RC_ZERO_3", &channelZero[3]},
	{"RC_ZERO_4", &channelZero[4]},
	{"RC_ZERO_5", &channelZero[5]},
	{"RC_ZERO_6", &channelZero[6]},
	{"RC_ZERO_7", &channelZero[7]},
	{"RC_MAX_0", &channelMax[0]},
	{"RC_MAX_1", &channelMax[1]},
	{"RC_MAX_2", &channelMax[2]},
	{"RC_MAX_3", &channelMax[3]},
	{"RC_MAX_4", &channelMax[4]},
	{"RC_MAX_5", &channelMax[5]},
	{"RC_MAX_6", &channelMax[6]},
	{"RC_MAX_7", &channelMax[7]},
	{"RC_ROLL", &rollChannel},
	{"RC_PITCH", &pitchChannel},
	{"RC_THROTTLE", &throttleChannel},
	{"RC_YAW", &yawChannel},
	{"RC_MODE", &modeChannel},
	// wifi
	{"WIFI_MODE", &wifiMode},
	{"WIFI_PORT_LOC", &udpLocalPort},
	{"WIFI_PORT_REM", &udpRemotePort},
	{"WIFI_LONG_RANGE", &wifiLongRange},
	{"WIFI_BROADCAST", &wifiBroadcast},
	// espnow
	{"ESPNOW_CHANNEL", &espnowChannel},
	// mavlink
	{"MAV_SYS_ID", &mavlinkSysId},
	{"MAV_RATE_SLOW", &telemetrySlow.rate},
	{"MAV_RATE_ATT", &telemetryAttitude.rate},
	{"MAV_RATE_RC", &telemetryRC.rate},
	{"MAV_RATE_MOT", &telemetryMotors.rate},
	{"MAV_RATE_IMU", &telemetryIMU.rate},
	// power
	{"PWR_VOLT_PIN", &voltagePin, setupPower},
	{"PWR_VOLT_SCALE", &voltageScale},
	{"PWR_VOLT_LPF_A", &voltageFilter.alpha},
	// safety
	{"SF_RC_LOSS_TIME", &rcLossTimeout},
	{"SF_DESCEND_TIME", &descendTime},
	{"SF_DISARM_TILT", &disarmTilt},
	// 【R10 新增】指示灯与蜂鸣器（耀坤 S3 板实测引脚，改板型时只改这里）
	{"LED_R_PIN", &ledRedPin, setupLED},
	{"LED_G_PIN", &ledGreenPin, setupLED},
	{"LED_B_PIN", &ledBluePin, setupLED},
	{"LED_LOW_VOLT", &ledLowVoltage},
	{"BUZ_PIN", &buzzerPin, setupLED},
	{"BUZ_ENABLE", &buzzerEnable},
};

void setupParameters() {
	print(TXT_SETUP_PARAMS);
	setDefaults();
	storage.begin("flix");
	// Read parameters from storage
	for (auto &parameter : parameters) {
		parameter.initial = parameter.getValue();
		if (storage.isKey(parameter.name)) {
			parameter.setValue(storage.getFloat(parameter.name));
		}
		parameter.cache = parameter.getValue();
	}
}

int parametersCount() {
	return sizeof(parameters) / sizeof(parameters[0]);
}

const char *getParameterName(int index) {
	if (index < 0 || index >= parametersCount()) return "";
	return parameters[index].name;
}

float getParameter(int index) {
	if (index < 0 || index >= parametersCount()) return NAN;
	return parameters[index].getValue();
}

float getParameter(const char *name) {
	for (auto &parameter : parameters) {
		if (strcasecmp(parameter.name, name) == 0) {
			return parameter.getValue();
		}
	}
	return NAN;
}

bool setParameter(const char *name, const float value) {
	for (auto &parameter : parameters) {
		if (strcasecmp(parameter.name, name) == 0) {
			if (parameter.integer && !isfinite(value)) return false; // can't set integer to NaN or Inf
			parameter.setValue(value);
			if (parameter.callback) parameter.callback();
			return true;
		}
	}
	return false;
}

void syncParameters() {
	static Rate rate(1);
	if (!rate) return; // sync once per second
	if (motorsActive()) return; // don't use flash while flying, it may cause a delay

	for (auto &parameter : parameters) {
		if (floatEquals(parameter.getValue(), parameter.cache)) continue; // no change

		storage.putFloat(parameter.name, parameter.getValue());
		parameter.cache = parameter.getValue(); // update cache
	}
}

void printParameters(const char *filter) {
	print(TXT_PARAM_HEADER);
	for (auto &parameter : parameters) {
		if (strncasecmp(parameter.name, filter, strlen(filter))) continue;

		if (floatEquals(parameter.getValue(), parameter.initial)) { // parameter changed
			print("%-15s  %-13g\n", parameter.name, parameter.getValue());
		} else {
			print("%-15s  %-13g  [%g]\n", parameter.name, parameter.getValue(), parameter.initial);
		}
	}
}

void resetParameters() {
	print(TXT_PRESET_DONE);
	storage.clear();
	ESP.restart();
}
