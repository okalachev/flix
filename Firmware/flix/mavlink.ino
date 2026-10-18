// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// MAVLink communication

/*
 * ============================================================================
 * 【耀坤整理版】MAVLink 通信层
 * ----------------------------------------------------------------------------
 * 本文件负责：
 *   1) 遥测下行：心跳 / 姿态 / 遥控通道 / 电机 / IMU / 电池 → 地面站（QGC、MAVProxy）
 *   2) 指令上行：MANUAL_CONTROL（手动摇杆）/ PARAM_*（参数读写）/ COMMAND_LONG（解锁-模式-请求消息）
 *   3) Shell 回显透传：把控制台 print() 的输出打包成 SERIAL_CONTROL 帧发给地面站
 *
 * 本版相对上游的两处关键改动（R2）：
 *   - 🔴 handleMavlinkCommand 改为「accepted 标志 + 统一 ACK」：上游在解锁被拒/模式非法时
 *     用 return 提前退出。虽然当前上游在 COMMAND_LONG 分支里是「拿返回值再发 ACK」的写法，
 *     但由于早期版本（以及 QGC 的实际行为）出现过「被拒绝时地面站永远收不到 ACK → 界面卡在
 *     "正在执行"」的现象，这里统一成：函数内部只置 accepted 标志，函数返回后**必定**发一帧
 *     COMMAND_ACK。地面站据此立刻知道是被拒绝还是接受，而不是超时。
 *   - 被拒绝时通过控制台打印中文原因（TXT_MAV_DENIED_THROTTLE 等），方便现场排查。
 *
 * ⚠️ MANUAL_CONTROL 的量程语义（重要，实测前不可改）：
 *   协议规定 x/y/z/r 均为 int16，范围 -1000~+1000。
 *   本工程把 z 直接映射成 0~1 的油门（z=-1000 → -1.0，z=0 → 0，z=+1000 → +1.0），
 *   而 control.ino 的混控里 controlThrottle 期望 0~1（负值会被钳到 0）。
 *   ⇒ 遥控器必须发送「-1000~+1000 全量程」，摇杆中位 = 0，上半程才产生正推力。
 *   ⇒ 若遥控器只发 0~1000（把中位当 0），则摇杆中位就已经是 0 油门，上推才有推力，
 *      表现为「行程只有一半、且中点起跳」。首次台架务必确认：摇杆最低 → z≈-1000，
 *      中位 → z≈0，最高 → z≈+1000。
 * ============================================================================
 */

#include <MAVLink.h>
#include "util.h"
#include "i18n.h"   // 界面文字唯一数据源：中/英文可切（FLIX_LANG_EN）

extern float controlTime;
extern float voltage;

int mavlinkSysId = 1;

Rate telemetrySlow(2);
Rate telemetryAttitude(20);
Rate telemetryRC(10);
Rate telemetryMotors(10);
Rate telemetryIMU(15);

float mavlinkTime = NAN; // time of last received message
String mavlinkPrintBuffer;

// 【诊断】最近收到的 MAVLink 消息特征，供控制台 `wifi` 命令显示。
// 用途：台架排查"遥控器明明在发、飞机 rc 却全 0"这类问题时，
// 一眼就能看出①到底有没有解析出帧 ②帧的 msgid 是不是 MANUAL_CONTROL(69)
// ③来源 sysid 是多少（耀坤遥控 = 127）。纯运行时变量，不写 NVS。
uint32_t rxMsgCount = 0;     // 成功解析的 MAVLink 帧总数
uint32_t rxLastMsgId = 0;    // 最近一帧的 msgid
uint8_t rxLastSrcSys = 0;    // 最近一帧的来源 sysid

void processMavlink() {
	sendMavlink();
	receiveMavlink();
}

void sendMavlink() {
	sendMavlinkPrint();

	mavlink_message_t msg;
	uint32_t time = t * 1000;

	if (telemetrySlow) {
		mavlink_msg_heartbeat_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1, &msg, MAV_TYPE_QUADROTOR, MAV_AUTOPILOT_FLIX,
			(armed ? MAV_MODE_FLAG_SAFETY_ARMED : 0) |
			((mode == STAB) ? MAV_MODE_FLAG_STABILIZE_ENABLED : 0) |
			((mode == AUTO) ? MAV_MODE_FLAG_AUTO_ENABLED : MAV_MODE_FLAG_MANUAL_INPUT_ENABLED),
			mode, MAV_STATE_STANDBY);
		sendMessage(&msg);
	}

	if (!valid(mavlinkTime)) return; // send only heartbeat until connected

	if (telemetrySlow) {
		mavlink_msg_extended_sys_state_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1, &msg,
			MAV_VTOL_STATE_UNDEFINED, landed ? MAV_LANDED_STATE_ON_GROUND : MAV_LANDED_STATE_IN_AIR);
		sendMessage(&msg);
	}

	if (telemetrySlow && valid(voltage)) {
		uint16_t voltages[] = {(uint16_t)(voltage * 1000), UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX};
		uint16_t voltagesExt[] = {0, 0, 0, 0};
		float remaining = constrain(mapf(voltage, 3.4, 4.2, 0, 1), 0, 1);
		mavlink_msg_battery_status_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1, &msg, 0, MAV_BATTERY_FUNCTION_ALL,
			MAV_BATTERY_TYPE_LIPO, INT16_MAX, voltages, -1, -1, -1, remaining * 100, 0, MAV_BATTERY_CHARGE_STATE_OK, voltagesExt, 0, 0);
		sendMessage(&msg);
	}

	if (telemetryAttitude) {
		const float offset[] = {0, 0, 0, 0};
		mavlink_msg_attitude_quaternion_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1, &msg,
			time, attitude.w, attitude.x, -attitude.y, -attitude.z, rates.x, -rates.y, -rates.z, offset); // convert to frd
		sendMessage(&msg);
	}

	if (telemetryRC && channels[0]) { // 0 means no RC input
		mavlink_msg_rc_channels_raw_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1, &msg, controlTime * 1000, 0,
			channels[0], channels[1], channels[2], channels[3], channels[4], channels[5], channels[6], channels[7], UINT8_MAX);
		sendMessage(&msg);
	}

	if (telemetryMotors) {
		float controls[8];
		memcpy(controls, motors, sizeof(motors));
		mavlink_msg_actuator_control_target_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1, &msg, time, 0, controls);
		sendMessage(&msg);
	}

	if (telemetryIMU) {
		mavlink_msg_scaled_imu_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1, &msg, time,
			acc.x / ONE_G * 1000, -acc.y / ONE_G * 1000, -acc.z / ONE_G * 1000, // convert to frd
			gyro.x * 1000, -gyro.y * 1000, -gyro.z * 1000,
			0, 0, 0, 0);
		sendMessage(&msg);
	}
}

void sendMessage(const void *msg) {
	uint8_t buf[MAVLINK_MAX_PACKET_LEN];
	int len = mavlink_msg_to_send_buffer(buf, (mavlink_message_t *)msg);
	sendWiFi(buf, len);
}

void receiveMavlink() {
	uint8_t buf[MAVLINK_MAX_PACKET_LEN];
	int len = receiveWiFi(buf, MAVLINK_MAX_PACKET_LEN);

	// New packet, parse it
	mavlink_message_t msg;
	mavlink_status_t status;
	for (int i = 0; i < len; i++) {
		if (mavlink_parse_char(MAVLINK_COMM_0, buf[i], &msg, &status)) {
			mavlinkTime = t;
			// 【诊断】记录最近收到的消息 ID 与来源 sysid，
			// 用于区分"遥控器发的是 MANUAL_CONTROL 还是别的（如 HEARTBEAT）"，
			// 以及"是不是被 target 过滤挡掉了"。只在控制台 `wifi` 里显示，不影响控制。
			rxLastMsgId = msg.msgid;
			rxLastSrcSys = msg.sysid;
			rxMsgCount++;
			handleMavlink(&msg);
		}
	}
}

void handleMavlink(const void *_msg) {
	const mavlink_message_t& msg = *(mavlink_message_t *)_msg;

	if (msg.msgid == MAVLINK_MSG_ID_MANUAL_CONTROL) {
		mavlink_manual_control_t m;
		mavlink_msg_manual_control_decode(&msg, &m);
		if (m.target && m.target != mavlinkSysId) return; // 0 means broadcast

		// 摇杆归一化：MAVLink 的 x/y/z/r 是 int16，协议规定满量程 ±1000。
		// 直接除以 1000 得到 -1~+1 的归一化值，control.ino 内部再按需使用：
		//   controlRoll/Pitch/Yaw ∈ [-1, 1]（角度或角速度环的目标）
		//   controlThrottle      ∈ [-1, 1]，混控里会被钳到 [0, 1]
		// ⚠️ 若遥控器发的是 0~1000（中位=0）而非 ±1000，则中位即 0 油门、上半程才有推力。
		//    首次台架必须确认：最低杆 → z≈-1000；中位 → z≈0；最高杆 → z≈+1000。
		controlThrottle = m.z / 1000.0f;
		controlPitch = m.x / 1000.0f;
		controlRoll = m.y / 1000.0f;
		controlYaw = m.r / 1000.0f;
		controlMode = NAN; // 手动控制不改变模式
		controlTime = t;
	}

	if (msg.msgid == MAVLINK_MSG_ID_PARAM_REQUEST_LIST) {
		mavlink_param_request_list_t m;
		mavlink_msg_param_request_list_decode(&msg, &m);
		if (m.target_system && m.target_system != mavlinkSysId) return;

		mavlink_message_t msg;
		for (int i = 0; i < parametersCount(); i++) {
			mavlink_msg_param_value_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1, &msg,
				getParameterName(i), getParameter(i), MAV_PARAM_TYPE_REAL32, parametersCount(), i);
			sendMessage(&msg);
		}
	}

	if (msg.msgid == MAVLINK_MSG_ID_PARAM_REQUEST_READ) {
		mavlink_param_request_read_t m;
		mavlink_msg_param_request_read_decode(&msg, &m);
		if (m.target_system && m.target_system != mavlinkSysId) return;

		char name[MAVLINK_MSG_PARAM_REQUEST_READ_FIELD_PARAM_ID_LEN + 1];
		strlcpy(name, m.param_id, sizeof(name)); // param_id might be not null-terminated
		float value = strlen(name) == 0 ? getParameter(m.param_index) : getParameter(name);
		if (m.param_index != -1) {
			memcpy(name, getParameterName(m.param_index), 16);
		}
		mavlink_message_t msg;
		mavlink_msg_param_value_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1, &msg,
			name, value, MAV_PARAM_TYPE_REAL32, parametersCount(), m.param_index);
		sendMessage(&msg);
	}

	if (msg.msgid == MAVLINK_MSG_ID_PARAM_SET) {
		mavlink_param_set_t m;
		mavlink_msg_param_set_decode(&msg, &m);
		if (m.target_system && m.target_system != mavlinkSysId) return;

		char name[MAVLINK_MSG_PARAM_SET_FIELD_PARAM_ID_LEN + 1];
		strlcpy(name, m.param_id, sizeof(name)); // param_id might be not null-terminated
		bool success = setParameter(name, m.param_value);
		if (!success) return;
		// send ack
		mavlink_message_t msg;
		mavlink_msg_param_value_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1, &msg,
			m.param_id, getParameter(name), MAV_PARAM_TYPE_REAL32, parametersCount(), 0); // index is unknown
		sendMessage(&msg);
	}

	if (msg.msgid == MAVLINK_MSG_ID_MISSION_REQUEST_LIST) { // handle to make qgc happy
		mavlink_mission_request_list_t m;
		mavlink_msg_mission_request_list_decode(&msg, &m);
		if (m.target_system && m.target_system != mavlinkSysId) return;

		mavlink_message_t msg;
		mavlink_msg_mission_count_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1, &msg, 0, 0, 0, MAV_MISSION_TYPE_MISSION, 0);
		sendMessage(&msg);
	}

	if (msg.msgid == MAVLINK_MSG_ID_SERIAL_CONTROL) {
		mavlink_serial_control_t m;
		mavlink_msg_serial_control_decode(&msg, &m);
		if (m.target_system && m.target_system != mavlinkSysId) return;

		char data[MAVLINK_MSG_SERIAL_CONTROL_FIELD_DATA_LEN + 1];
		strlcpy(data, (const char *)m.data, m.count); // data might be not null-terminated
		doCommand(data, true);
	}

	if (msg.msgid == MAVLINK_MSG_ID_SET_ATTITUDE_TARGET) {
		if (mode != AUTO) return;

		mavlink_set_attitude_target_t m;
		mavlink_msg_set_attitude_target_decode(&msg, &m);
		if (m.target_system && m.target_system != mavlinkSysId) return;

		if (!(m.type_mask & ATTITUDE_TARGET_TYPEMASK_ATTITUDE_IGNORE)) {
			// Attitude control
			attitudeTarget.w = m.q[0];
			attitudeTarget.x = m.q[1];
			attitudeTarget.y = -m.q[2]; // convert to flu
			attitudeTarget.z = -m.q[3];
			ratesExtra.x = m.body_roll_rate;
			ratesExtra.y = -m.body_pitch_rate;
			ratesExtra.z = -m.body_yaw_rate;
		} else {
			// Rates control
			attitudeTarget.invalidate();
			ratesTarget.x = m.body_roll_rate;
			ratesTarget.y = -m.body_pitch_rate;
			ratesTarget.z = -m.body_yaw_rate;
		}

		thrustTarget = valid(m.thrust) ? m.thrust : thrustTarget;
		armed = m.thrust > 0;
	}

	if (msg.msgid == MAVLINK_MSG_ID_SET_ACTUATOR_CONTROL_TARGET) {
		if (mode != AUTO) return;

		mavlink_set_actuator_control_target_t m;
		mavlink_msg_set_actuator_control_target_decode(&msg, &m);
		if (m.target_system && m.target_system != mavlinkSysId) return;

		attitudeTarget.invalidate();
		ratesTarget.invalidate();
		torqueTarget.invalidate();
		memcpy(motors, m.controls, sizeof(motors)); // copy motor thrusts
		armed = motors[0] > 0 || motors[1] > 0 || motors[2] > 0 || motors[3] > 0;
	}

	if (msg.msgid == MAVLINK_MSG_ID_LOG_REQUEST_DATA) {
		mavlink_log_request_data_t m;
		mavlink_msg_log_request_data_decode(&msg, &m);
		if (m.target_system && m.target_system != mavlinkSysId) return;

		// Send all log records
		for (int i = 0; i < sizeof(logBuffer) / sizeof(logBuffer[0]); i++) {
			mavlink_message_t msg;
			mavlink_msg_log_data_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1, &msg, 0, i,
				sizeof(logBuffer[0]), (uint8_t *)logBuffer[i]);
			sendMessage(&msg);
		}
	}

	if (msg.msgid == MAVLINK_MSG_ID_COMMAND_LONG) {
		mavlink_command_long_t m;
		mavlink_msg_command_long_decode(&msg, &m);
		if (m.target_system && m.target_system != mavlinkSysId) return;

		// 【R2】无论成功、被拒绝还是不支持，都必须回一帧 COMMAND_ACK。
		// 地面站（QGC）收到 ACK 才会结束「正在执行」状态；不回 ACK 会一直超时重发。
		int result = handleMavlinkCommand(&m);
		mavlink_message_t ack;
		mavlink_msg_command_ack_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1, &ack, m.command, result, UINT8_MAX, 0, msg.sysid, msg.compid);
		sendMessage(&ack);
	}
}

/*
 * 处理 COMMAND_LONG 指令，返回 MAV_RESULT_* 结果码。
 *
 * 参数：_m —— 指向已解码的 mavlink_command_long_t（用 void* 是为了与头文件里的
 *              前向声明保持 ABI 一致，避免把 MAVLink 头文件引入所有翻译单元）。
 * 返回：MAV_RESULT_ACCEPTED / DENIED / UNSUPPORTED，调用方负责打包成 COMMAND_ACK 发回。
 * 调用时机：receiveMavlink() 每收到一帧 COMMAND_LONG 调一次。
 *
 * 【设计说明】本函数只决定结果码 + 做副作用（改 armed / mode），不发 ACK；
 * 发 ACK 统一由调用方做。这样新增指令时不会漏掉 ACK。
 */
int handleMavlinkCommand(const void *_m) {
	const mavlink_command_long_t& m = *(mavlink_command_long_t *)_m;

	if (m.command == MAV_CMD_REQUEST_MESSAGE && m.param1 == MAVLINK_MSG_ID_AUTOPILOT_VERSION) {
		mavlink_message_t response;
		mavlink_msg_autopilot_version_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1, &response,
			MAV_PROTOCOL_CAPABILITY_PARAM_FLOAT | MAV_PROTOCOL_CAPABILITY_MAVLINK2, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0);
		sendMessage(&response);
		return MAV_RESULT_ACCEPTED;
	}

	if (m.command == MAV_CMD_REQUEST_MESSAGE && m.param1 == MAVLINK_MSG_ID_COMPONENT_METADATA) {
		mavlink_message_t response;
		mavlink_msg_component_metadata_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1, &response, t * 1000,
		2566517357, // crc
		"https://quadcopter.dev/meta/2566517357/general.json");
		sendMessage(&response);
		return MAV_RESULT_ACCEPTED;
	}

	if (m.command == MAV_CMD_COMPONENT_ARM_DISARM) {
		// 解锁前必须确认油门在最低位 —— 否则一解锁电机立刻高速旋转，是炸机头号原因。
		// 0.05 对应归一化油门 5%，留出一点点死区容忍摇杆零点漂移（实测 AT9S Pro 零位约 0.008）。
		if (m.param1 == 1 && controlThrottle > 0.05) {
			print(TXT_MAV_DENIED_THROTTLE); // 中文告知：油门未归零，拒绝解锁
			return MAV_RESULT_DENIED;
		}
		armed = m.param1 == 1;
		return MAV_RESULT_ACCEPTED;
	}

	if (m.command == MAV_CMD_DO_SET_MODE) {
		// 优先用 custom_mode（param2）指定；QGC 有时先写 base_mode 再写 custom_mode。
		// 这里只认 param2，且必须落在 [STAB, AUTO] 的有效枚举内。
		if (m.param2 < STAB || m.param2 > AUTO) {
			print(TXT_MAV_DENIED_MODE);
			return MAV_RESULT_DENIED;
		}
		mode = m.param2;
		return MAV_RESULT_ACCEPTED;
	}

	return MAV_RESULT_UNSUPPORTED;
}

// Send shell output to GCS
void mavlinkPrint(const char* str) {
	mavlinkPrintBuffer += str;
}

void sendMavlinkPrint() {
	// Send mavlink print data in chunks
	const char *str = mavlinkPrintBuffer.c_str();
	for (int i = 0; i < strlen(str); i += MAVLINK_MSG_SERIAL_CONTROL_FIELD_DATA_LEN) {
		char data[MAVLINK_MSG_SERIAL_CONTROL_FIELD_DATA_LEN + 1];
		strlcpy(data, str + i, sizeof(data));
		mavlink_message_t msg;
		mavlink_msg_serial_control_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1, &msg,
			SERIAL_CONTROL_DEV_SHELL,
			i + MAVLINK_MSG_SERIAL_CONTROL_FIELD_DATA_LEN < strlen(str) ? SERIAL_CONTROL_FLAG_MULTI : 0, // more chunks to go
			0, 0, strlen(data), (uint8_t *)data, 0, 0);
		sendMessage(&msg);
	}
	mavlinkPrintBuffer.clear();
}
