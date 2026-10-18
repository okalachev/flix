// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix
// 耀坤整理版修改：https://github.com/YaoKunS/flix

// 时间基准与主循环节拍

// ---------------------------------------------------------------------------
// 【为什么改成 64 位微秒累加器】—— 修「上电约 71.6 分钟后遥测全停」
//
// 现象：飞机连续上电约 71.6 分钟后，串口敲命令**有回显**，但姿态不再更新、
//   一帧遥测都没有；reboot 或拔插数据线立刻恢复。
//
// 根因：原实现 `t = micros() / 1e6`，而 ESP32 的 micros() 是 **32 位微秒**计数，
//   每 2^32 us = 4294967296 us，约 4294.97 秒 = **71.6 分钟回绕一次**。
//   回绕瞬间 t 从 4295 直接跳回 ~0（**倒退**）。而全工程所有"按时间放行"的门
//   都写成 `t - last >= 周期`（mavlink.ino 的 5 个遥测 Rate、parameters/power/log
//   的 Rate、这里的 1 秒统计窗口）—— t 一倒退，差值变负，这些门**集体停止放行**，
//   且要等 t 重新爬回原值（最多再 71.6 分钟）才自愈。这正好解释"有回显
//   （print 不受门控）但遥测全死"的割裂现象。
//
// 修法：维护一个 64 位累加器 timeUs，每步累加 `nowMicros - lastMicros`
//   的**无符号差值**。C++ 无符号减法在回绕时是良定义的：32 位回绕自动抵消，
//   得到的必然是本步真实微秒差（前提：本函数至少每 71.6 分钟被调用一次，
//   实际是近 1 kHz，绝对满足）。64 位可跑 2^64 us，实际永不回绕。
//   附带好处：dt 直接由微秒差换算，比原来"两个大浮点相减"更精确。
//
// 探针 tRewinds：记录原始 micros() 的回绕次数。修好之后它**仍可能 >0**，
//   但那只表示"硬件回绕发生过且已被吸收"，不再影响任何门控 ——
//   用它来验收"跨过 71.6 分钟遥测照样在跑"。
// ---------------------------------------------------------------------------

float t = NAN;      // 当前时间，秒（**单调递增**，不再随 micros 回绕倒退）
float dt;           // 与上一步的时间差，秒
float loopRate;     // 实测主循环频率，Hz

static uint64_t timeUs = 0;     // 累计微秒（64 位，永不回绕）
static uint32_t lastMicros = 0; // 上一次原始 micros() 值，用于无符号求差

uint32_t tRewinds = 0; // 探针：原始 micros() 回绕次数（已被吸收，无害）
float tRewindAt = 0;   // 探针：最近一次回绕发生时的 t（秒），正常应 ≈4295 s

void step() {
	const uint32_t nowMicros = micros();
	const uint32_t deltaUs = nowMicros - lastMicros; // 无符号减法：32 位回绕自动抵消

	if (nowMicros < lastMicros) { // 抓到一次原始 micros() 回绕（差值本身已正确）
		tRewinds++;
		tRewindAt = t;
	}
	lastMicros = nowMicros;
	timeUs += deltaUs; // 64 位累加 ⇒ t 单调递增

	const bool firstStep = !valid(t); // 首圈 t 仍是 NAN
	t = timeUs / 1000000.0f;
	dt = firstStep ? 0 : (float)deltaUs / 1000000.0f;

	if (!(dt > 0)) {
		dt = 0; // 首圈或复位后把 dt 归零，避免把"上电到第一次 step 的时长"喂给 PID
	}

	computeLoopRate();
}

void computeLoopRate() {
	static float windowStart = 0;
	static uint32_t rate = 0;
	rate++;
	if (t - windowStart >= 1) { // 1 秒统计窗口
		loopRate = rate;
		windowStart = t;
		rate = 0;
	}
}
