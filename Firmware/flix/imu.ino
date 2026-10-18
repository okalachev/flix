// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix
// 耀坤整理版修改：https://github.com/YaoKunS/flix

// 惯性测量单元（IMU）驱动与校准
//
// 【耀坤整理版的改动】
//   1) 出厂默认带耀坤 S3 的引脚与型号（上游原版是 -1 / 未分配，直接用会崩）。
//   2) 新增上电自动识别（I2C 先探、SPI 再探），排母换模块不用改代码。
//   3) 朝向按**总线**分档：I2C(6050 系) +π/2，SPI(6500 系) −π/2。
//   4) 全部提示文字中文（i18n.h）。
//   5) imu 不可用时不再死等：上游 readIMU() 直接调 imu->waitForData()，
//      一旦探测失败拿到 nullptr 就是整机卡死（表现为"上电无输出、像砖头"）。

#include <SPI.h>
#include <Wire.h>
#include <FlixPeriph.h>
#include "vector.h"
#include "filter.h"
#include "util.h"
#include "i18n.h"

IMU *imu;
int imuAutoDetect = 1; // 1 - 上电自动识别（默认）；0 - 只用下面的 IMU_MODEL 手动值
int imuModel = -1;     // 1 - MPU9250/MPU6500, 2 - ICM20948, 3 - MPU6050, 4 - ICM40609D, -1 待识别
int imuBus = 0;        // 0 - SPI, 1 - I2C
int imuRotLock = 0;    // 1 - 自动识别不覆盖 IMU_ROT_YAW（现场锁定自定义朝向时用）
bool imuOK = false;    // IMU 是否就绪；false 时禁止解锁，且主循环不阻塞
const char *imuDetectNote = "manual"; // 自动识别的依据（I2C id / SPI id / manual），供 imu 命令回显

// 耀坤 S3 实测引脚（以《耀坤S3无人机引脚定义.xlsx》V2 版为准，V1 老板靠自动扫描兼容）：
//   MPU6500 走 SPI：SCK=38 / MISO=21 / MOSI=18；CS 新板=42、老板实测=17（探测候选表两者都试）
//   MPU6050 走 I2C0：SDA=9 / SCL=8（表格与实物一致，2026-10-18 用户确认"按表格没错"）
int imuSckPin = 38, imuMisoPin = 21, imuMosiPin = 18, imuCsPin = 17, imuIntPin = -1;
// 【坑】这里曾按错误记忆写成 SDA=8 / SCL=9（与表格相反），导致 2 号机 I2C 模块探不到。
// 引脚表与实际布线冲突时以实测为准，但本次实测证明表格是对的 —— 恢复 SDA=9 / SCL=8。
int imuSdaPin = 9, imuSclPin = 8;

// 默认朝向（SPI 档）。识别成功后 applyIMUProfile() 会按总线覆盖。
Vector imuRotation(0, 0, -PI / 2);

Vector gyro; // 角速度，rad/s
Vector gyroBias;

Vector acc; // 加速度，m/s^2
Vector accBias;
Vector accScale(1, 1, 1);

LowPassFilter<Vector> gyroBiasFilter(0.001);

// ---------------------------------------------------------------------------
// IMU 自动识别
//
// 判别依据 = WHO_AM_I 寄存器 0x75：
//   0x68 / 0x34 / 0x38 / 0x0C / 0x3A -> MPU6050 系（含部分国产克隆片）model=3, I2C
//   0x70 / 0x71 / 0x73               -> MPU6500 / MPU9250 / MPU9255   model=1, SPI
//
// 探测顺序：先 I2C（有 ACK 机制，不会误判）→ 再 SPI（悬空可能读到噪声，靠白名单兜底）。
// 只在 setup() 里跑一次，带超时，绝不进入 1 kHz 主循环。
// 识别结果只放内存、不写 NVS —— 排母随时可换模块，每次上电重探才符合直觉。
// ---------------------------------------------------------------------------

static bool isMpu6050Id(int who) {
	return who == 0x68 || who == 0x34 || who == 0x38 || who == 0x0C || who == 0x3A;
}

static bool isMpu9x50Id(int who) {
	return who == 0x70 || who == 0x71 || who == 0x73;
}

// 裸 I2C 读 WHO_AM_I（不依赖 FlixPeriph 内部实现，避免版本差异）
static int probeI2C(uint8_t addr) {
	Wire.begin(imuSdaPin, imuSclPin);
	Wire.setTimeOut(20);
	Wire.beginTransmission(addr);
	Wire.write(0x75);
	if (Wire.endTransmission(false) != 0) return -1; // 无 ACK
	if (Wire.requestFrom((uint16_t)addr, (size_t)1) != 1) return -1;
	if (!Wire.available()) return -1;
	return Wire.read();
}

// 裸 SPI 读 WHO_AM_I（MPU9x50 的 SPI 为 MODE3，读寄存器需置 bit7）
static int probeSPI(int cs) {
	if (cs < 0) return -1;
	SPI.begin(imuSckPin, imuMisoPin, imuMosiPin);
	pinMode(cs, OUTPUT);
	digitalWrite(cs, HIGH);
	delayMicroseconds(10);
	SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE3));
	digitalWrite(cs, LOW);
	delayMicroseconds(10);
	SPI.transfer(0x75 | 0x80); // 读位
	const uint8_t who = SPI.transfer(0x00);
	digitalWrite(cs, HIGH);
	SPI.endTransaction();
	return who;
}

// 按识别出的型号设置总线与安装朝向
//
// 【朝向为什么按总线分档，而不按 WHO_AM_I】
//   6050 模块插 8P 排母：丝印 X → 机体左、Y → 机尾  ⇒ Rz(+90度)
//   6500 模块插 10P 排母：丝印 X → 机体右、Y → 机头 ⇒ Rz(-90度)
//   180 度差异来自**模块插入方向相反**，不是芯片差异。
//   而且国产 6050 兼容片 WHO_AM_I 可能读成 0x70（I2C），若按"0x70→6500 档"
//   就会差 180 度（实测：姿态球 roll/pitch 同时反）⇒ 分档依据只能是总线。
static void applyIMUProfile(int model, int bus, int who, const char *why) {
	imuModel = model;
	imuBus = bus;
	imuDetectNote = why;

	const Vector detected = (bus == 1) ? Vector(0, 0, PI / 2) : Vector(0, 0, -PI / 2);
	if (!imuRotLock) imuRotation = detected;

	if (bus == 1) {
		print(TXT_IMU_FOUND_I2C, imuSdaPin, imuSclPin);
	} else {
		print(TXT_IMU_FOUND_SPI, imuSckPin, imuMisoPin, imuMosiPin, imuCsPin);
	}
	if (who >= 0) print(TXT_IMU_WHOAMI2, who);
	print(TXT_IMU_ROT_APPLIED, imuRotation.z, why);
}

static bool detectIMU() {
	print(TXT_IMU_DETECTING);

	// ① I2C0 —— 全总线扫描（0x08~0x77），不再只试 0x68/0x69
	// 【为什么要扫全总线】市面上 MPU6050 模块的 I2C 地址理论上是 0x68/0x69（AD0 决定），
	// 但耀坤采购的部分模块丝印/资料标注非标地址（如 0x08），固定探测表会漏。
	// 做法：逐地址 ACK 探测，对每个有应答的地址读 WHO_AM_I：
	//   - 读到合法 MPU id（6050 系或 6500 系克隆片）→ 直接采用；
	//   - 有 ACK 但 WHO 不合法（0x00/0xFF/其他外设）→ 记为兜底候选（可能是识别表没覆盖的兼容片）；
	//   - 0x68/0x69 优先级最高（标准地址最可信），其余按地址从小到大。
	// V1/V2 PCB 的 I2C0 布线相同（SDA=9/SCL=8），无需按 PCB 版本区分。
	int who = -1;
	uint8_t foundAddr = 0;
	uint8_t fallbackAddr = 0;        // 有 ACK 但 WHO 不合法的兜底候选
	{
		Wire.begin(imuSdaPin, imuSclPin);
		Wire.setTimeOut(20);
		for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
			Wire.beginTransmission(addr);
			if (Wire.endTransmission() != 0) continue;   // 无 ACK，下一地址
			// 有 ACK：尝试读 WHO_AM_I（0x75）
			Wire.beginTransmission(addr);
			Wire.write(0x75);
			int r = Wire.endTransmission(false);
			int w = -1;
			if (r == 0 && Wire.requestFrom((uint16_t)addr, (size_t)1) == 1 && Wire.available())
				w = Wire.read();
			if (isMpu6050Id(w) || isMpu9x50Id(w)) { who = w; foundAddr = addr; break; }
			if (fallbackAddr == 0) fallbackAddr = addr;  // 只记第一个兜底候选
		}
	}
	if (who < 0 && fallbackAddr != 0) {
		// 没找到合法 id，但有地址在应答 —— 可能是识别表外的国产兼容片，赌一把
		// （0x68/0x69 之外的地址一律按 6050 系处理，setupIMU() 里 begin 失败还会换型号重试）
		who = probeI2C(fallbackAddr);
		foundAddr = fallbackAddr;
	}
	if (who >= 0) {
		// 注意：I2C（8P 排母）上只会插 6050 系模块 —— 6500 走 10P/SPI 排母，物理上插不进来。
		// 型号选择：0x70/0x71/0x73（6500 系克隆片）→ model=1；其余（含标准 6050 id 与
		// 全总线扫描兜底读到的非法 id 0x00）→ model=3（MPU6050）。
		// 选错时 setupIMU() 里 begin 失败还会换型号重试，不会死在这。
		const int model = isMpu9x50Id(who) ? 1 : 3;
		applyIMUProfile(model, 1, who, isMpu9x50Id(who) ? "I2C id（6050 系克隆片）" : "I2C id");
		return true;
	}

	// ② SPI —— MPU6500 / MPU9250 / MPU9255
	// 【兼容新旧 PCB，无需重编】V1 老板 MPU6500 片选实测接 GPIO17（INT 也在 17）；
	// V2 新板按《耀坤S3无人机引脚定义.xlsx》：MPU6500_CS=GPIO42、MPU6500_INT=GPIO40（INT 本工程不用，定时器采样）。
	// 所以按候选表 {17, 42} 依次探测，谁读到合法 WHO_AM_I 就用谁 —— 两个版本的板子都能自动适配。
	int foundCs = -1, foundWho = -1, looseCs = -1, looseWho = -1;
	const int csCandidates[3] = { imuCsPin, 42, 17 };
	for (int k = 0; k < 3; k++) {
		const int cs = csCandidates[k];
		if (cs < 0) continue;
		bool seen = false;
		for (int j = 0; j < k; j++) if (csCandidates[j] == cs) seen = true;
		if (seen) continue;

		const int who = probeSPI(cs);
		print(TXT_IMU_PROBE_CS, cs, who);
		if (isMpu9x50Id(who)) { foundCs = cs; foundWho = who; break; }
		// 非标 ID 的国产克隆片：留个候选，交给 begin() 里的状态判定兜底
		if (who != 0x00 && who != 0xFF && looseCs < 0) { looseCs = cs; looseWho = who; }
	}
	if (foundCs < 0 && looseCs >= 0) { foundCs = looseCs; foundWho = looseWho; }
	if (foundCs >= 0) {
		imuCsPin = foundCs; // 记住真正可用的片选，后续 createIMU() 与打印都用它
		applyIMUProfile(1, 0, foundWho, "SPI id");
		return true;
	}

	print(TXT_IMU_NOT_FOUND);
	return false;
}

void createIMU() {
	if (imuBus == 0) {
		// SPI 连接
		SPI.begin(imuSckPin, imuMisoPin, imuMosiPin);
		imu = IMU::create(imuModel, SPI, imuCsPin, imuIntPin);
	} else {
		// I2C 连接
		Wire.begin(imuSdaPin, imuSclPin);
		imu = IMU::create(imuModel, Wire, imuIntPin);
	}
}

void setupIMU() {
	print(TXT_SETUP_IMU);
	free(imu);
	imu = nullptr;

	if (imuAutoDetect) detectIMU();

	if (imuModel == 3) imuBus = 1; // MPU6050 只能走 I2C
	if (imuModel < 0) {
		// 没识别出来且没有手动值：先按 MPU6050/I2C 试，避免 IMU::create(-1) 返回 nullptr
		imuModel = 3;
		imuBus = 1;
	}

	createIMU();
	if (imu != nullptr) imu->begin();

	// 首次启动失败：自动模式下换另一种型号再试一次（兜住国产克隆片 / 探测误判）
	if (imu != nullptr && imu->status() != 0 && imuAutoDetect) {
		print(TXT_IMU_RETRY, imuModel, imuBus, imu->status());
		free(imu);
		const int fallback = (imuModel == 3) ? 1 : 3;
		applyIMUProfile(fallback, fallback == 3 ? 1 : 0, -1, "fallback");
		createIMU();
		if (imu != nullptr) imu->begin();
		print(TXT_IMU_RETRY_RESULT, imuModel, imuBus, imu == nullptr ? -1 : imu->status());
	}

	if (imu == nullptr || imu->status() != 0) {
		// 【为什么要这个分支】上游没有它：探测失败时 imu 是 nullptr，
		// readIMU() 里 imu->waitForData() 直接空指针崩溃（表现为上电即死、看门狗重启循环，
		// 串口只有半行输出）。这里改为"标记不可用 + 禁止解锁"，飞机仍能连上、能看日志。
		imuOK = false;
		print(TXT_IMU_UNUSABLE);
		return;
	}

	imuOK = true;
	print(TXT_IMU_READY, imu->getModel(), imuBus == 0 ? "SPI" : "I2C");
	configureIMU();
}

void configureIMU() {
	imu->setAccelRange(IMU::ACCEL_RANGE_4G);
	imu->setGyroRange(IMU::GYRO_RANGE_2000DPS);
	imu->setDLPF(IMU::DLPF_MAX);
	imu->setRate(IMU::RATE_1KHZ_APPROX);
	imu->setupInterrupt();
}

void readIMU() {
	if (!imuOK || imu == nullptr) return; // 不可用时直接返回：waitForData() 会死等，绝不能进
	imu->waitForData();
	imu->getGyro(gyro.x, gyro.y, gyro.z);
	imu->getAccel(acc.x, acc.y, acc.z);
	calibrateGyroOnce();

	// 应用比例与零偏
	acc = (acc - accBias) / accScale;
	gyro = gyro - gyroBias;

	// 旋转到机体坐标系
	Quaternion rotation = Quaternion::fromEuler(imuRotation);
	acc = Quaternion::rotateVector(acc, rotation.inversed());
	gyro = Quaternion::rotateVector(gyro, rotation.inversed());
}

void calibrateGyroOnce() {
	static Delay landedDelay(2);
	if (!landedDelay.update(landed)) return; // 只在确认静止时校准

	gyroBias = gyroBiasFilter.update(gyro);
}

// 六面校准的极值累加器
// 【🔴 必须每轮重置，曾经是 bug 根源（2026-10-18 台架实测发现）】
// 原写法把这两个变量放在 calibrateAccelOnce() 里当 `static`：
//     static Vector accMax(-INFINITY, -INFINITY, -INFINITY);
//     static Vector accMin(INFINITY, INFINITY, INFINITY);
// C++ 的 static 局部变量**只在第一次调用时初始化**，之后再进来保留上次的值。
// 后果：第二次执行 `ca` 时，accMax/accMin 还是上一轮（甚至上电以来）的残留，
// 于是 accScale = (accMax-accMin)/2/G 完全失真 —— 实测算出 0.0002（应≈1.0），
// 加速度读数随之变成 -0.86/0.19/9.19（应≈0/0/9.8），姿态彻底不准。
// 修法：提成文件级全局，在 calibrateAccel() 开头显式重置。
static Vector accCalMax(-INFINITY, -INFINITY, -INFINITY);
static Vector accCalMin(INFINITY, INFINITY, INFINITY);

void calibrateAccel() {
	if (!imuOK || imu == nullptr) {
		print(TXT_IMU_CAL_UNAVAIL);
		return;
	}
	print(TXT_CAL_START);
	imu->setAccelRange(IMU::ACCEL_RANGE_2G); // 最灵敏档

	// 【关键】每轮校准开始前清空极值累加器，否则会累加上一次的结果
	accCalMax = Vector(-INFINITY, -INFINITY, -INFINITY);
	accCalMin = Vector(INFINITY, INFINITY, INFINITY);

	print(TXT_CAL_1);
	pause(8);
	calibrateAccelOnce();
	print(TXT_CAL_2);
	pause(8);
	calibrateAccelOnce();
	print(TXT_CAL_3);
	pause(8);
	calibrateAccelOnce();
	print(TXT_CAL_4);
	pause(8);
	calibrateAccelOnce();
	print(TXT_CAL_5);
	pause(8);
	calibrateAccelOnce();
	print(TXT_CAL_6);
	pause(8);
	calibrateAccelOnce();

	printIMUCalibration();
	print(TXT_CAL_DONE);
	configureIMU();
}

void calibrateAccelOnce() {
	const int samples = 1000;

	// 求加速度计读数的均值
	acc = Vector(0, 0, 0);
	for (int i = 0; i < samples; i++) {
		imu->waitForData();
		Vector sample;
		imu->getAccel(sample.x, sample.y, sample.z);
		acc = acc + sample;
	}
	acc = acc / samples;

	// 更新六面校准的极值（累加器在 calibrateAccel() 开头已重置，见那里的说明）
	if (acc.x > accCalMax.x) accCalMax.x = acc.x;
	if (acc.y > accCalMax.y) accCalMax.y = acc.y;
	if (acc.z > accCalMax.z) accCalMax.z = acc.z;
	if (acc.x < accCalMin.x) accCalMin.x = acc.x;
	if (acc.y < accCalMin.y) accCalMin.y = acc.y;
	if (acc.z < accCalMin.z) accCalMin.z = acc.z;

	// 求比例与零偏
	accScale = (accCalMax - accCalMin) / 2 / ONE_G;
	accBias = (accCalMax + accCalMin) / 2;
}

void printIMUCalibration() {
	print(TXT_CAL_GYRO_BIAS, gyroBias.x, gyroBias.y, gyroBias.z);
	print(TXT_CAL_ACC_BIAS, accBias.x, accBias.y, accBias.z);
	print(TXT_CAL_ACC_SCALE, accScale.x, accScale.y, accScale.z);
}

void printIMUInfo() {
	if (imu == nullptr) {
		print(TXT_IMU_NOT_FOUND_SHORT);
		return;
	}
	imu->status() ? print(TXT_IMU_STATUS_ERR, imu->status()) : print(TXT_IMU_STATUS_OK);
	print(TXT_IMU_MODEL, imu->getModel());
	print(TXT_IMU_WHOAMI, imu->whoAmI());
	print(TXT_IMU_SUMMARY, imuBus == 0 ? "SPI" : "I2C0", imuDetectNote, imuOK);
	if (imuBus == 0) {
		print(TXT_IMU_PINS_SPI, imuSckPin, imuMisoPin, imuMosiPin, imuCsPin, imuIntPin);
	} else {
		print(TXT_IMU_PINS_I2C, imuSdaPin, imuSclPin, imuIntPin);
	}
	print(TXT_IMU_RATE, loopRate);
	print(TXT_IMU_INT_MODE, imuIntPin != -1 ? TXT_IMU_PIN_STR : TXT_IMU_PIN_TIMER);
	print(TXT_IMU_TEMP, imu->getTemp());
	print(TXT_IMU_GYRO, gyro.x, gyro.y, gyro.z);
	print(TXT_IMU_ACC, acc.x, acc.y, acc.z);
	imu->waitForData();
	Vector rawGyro, rawAcc;
	imu->getGyro(rawGyro.x, rawGyro.y, rawGyro.z);
	imu->getAccel(rawAcc.x, rawAcc.y, rawAcc.z);
	print(TXT_IMU_RAW_GYRO, rawGyro.x, rawGyro.y, rawGyro.z);
	print(TXT_IMU_RAW_ACC, rawAcc.x, rawAcc.y, rawAcc.z);
}
