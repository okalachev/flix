// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// MAVLink communication

#if WIFI_ENABLED

#include <MAVLink.h>

#define SYSTEM_ID 1
#define PERIOD_SLOW 1.0
#define PERIOD_FAST 0.1
#define MAVLINK_CONTROL_YAW_DEAD_ZONE 0.1f

String mavlinkPrintBuffer;

extern double controlTime;
extern float controlRoll, controlPitch, controlThrottle, controlYaw, controlMode;

void processMavlink() {
	sendMavlink();
	receiveMavlink();
}

void sendMavlink() {
	sendMavlinkPrint();

	static double lastSlow = 0;
	static double lastFast = 0;

	mavlink_message_t msg;
	uint32_t time = t * 1000;

	if (t - lastSlow >= PERIOD_SLOW) {
		lastSlow = t;

		mavlink_msg_heartbeat_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg, MAV_TYPE_QUADROTOR, MAV_AUTOPILOT_GENERIC,
			(armed ? MAV_MODE_FLAG_SAFETY_ARMED : 0) |
			((mode == STAB) ? MAV_MODE_FLAG_STABILIZE_ENABLED : 0) |
			((mode == AUTO) ? MAV_MODE_FLAG_AUTO_ENABLED : MAV_MODE_FLAG_MANUAL_INPUT_ENABLED),
			mode, MAV_STATE_STANDBY);
		sendMessage(&msg);

		mavlink_msg_extended_sys_state_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg,
			MAV_VTOL_STATE_UNDEFINED, landed ? MAV_LANDED_STATE_ON_GROUND : MAV_LANDED_STATE_IN_AIR);
		sendMessage(&msg);
	}

	if (t - lastFast >= PERIOD_FAST) {
		lastFast = t;

		const float zeroQuat[] = {0, 0, 0, 0};
		mavlink_msg_attitude_quaternion_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg,
			time, attitude.w, attitude.x, -attitude.y, -attitude.z, rates.x, -rates.y, -rates.z, zeroQuat); // convert to frd
		sendMessage(&msg);

		mavlink_msg_rc_channels_raw_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg, controlTime * 1000, 0,
			channels[0], channels[1], channels[2], channels[3], channels[4], channels[5], channels[6], channels[7], UINT8_MAX);
		if (channels[0] != 0) sendMessage(&msg); // 0 means no RC input

		float controls[8];
		memcpy(controls, motors, sizeof(motors));
		mavlink_msg_actuator_control_target_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg, time, 0, controls);
		sendMessage(&msg);

		mavlink_msg_scaled_imu_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg, time,
			acc.x * 1000, -acc.y * 1000, -acc.z * 1000, // convert to frd
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
			handleMavlink(&msg);
		}
	}
}

void handleMavlink(const void *_msg) {
	const mavlink_message_t& msg = *(mavlink_message_t *)_msg;

	if (msg.msgid == MAVLINK_MSG_ID_MANUAL_CONTROL) {
		mavlink_manual_control_t m;
		mavlink_msg_manual_control_decode(&msg, &m);
		if (m.target && m.target != SYSTEM_ID) return; // 0 is broadcast

		controlThrottle = m.z / 1000.0f;
		controlPitch = m.x / 1000.0f;
		controlRoll = m.y / 1000.0f;
		controlYaw = m.r / 1000.0f;
		controlMode = NAN;
		controlTime = t;

		if (abs(controlYaw) < MAVLINK_CONTROL_YAW_DEAD_ZONE) controlYaw = 0;
	}

	if (msg.msgid == MAVLINK_MSG_ID_PARAM_REQUEST_LIST) {
		mavlink_param_request_list_t m;
		mavlink_msg_param_request_list_decode(&msg, &m);
		if (m.target_system && m.target_system != SYSTEM_ID) return;

		mavlink_message_t msg;
		for (int i = 0; i < parametersCount(); i++) {
			mavlink_msg_param_value_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg,
				getParameterName(i), getParameter(i), MAV_PARAM_TYPE_REAL32, parametersCount(), i);
			sendMessage(&msg);
		}
	}

	if (msg.msgid == MAVLINK_MSG_ID_PARAM_REQUEST_READ) {
		mavlink_param_request_read_t m;
		mavlink_msg_param_request_read_decode(&msg, &m);
		if (m.target_system && m.target_system != SYSTEM_ID) return;

		char name[MAVLINK_MSG_PARAM_REQUEST_READ_FIELD_PARAM_ID_LEN + 1];
		strlcpy(name, m.param_id, sizeof(name)); // param_id might be not null-terminated
		float value = strlen(name) == 0 ? getParameter(m.param_index) : getParameter(name);
		if (m.param_index != -1) {
			memcpy(name, getParameterName(m.param_index), 16);
		}
		mavlink_message_t msg;
		mavlink_msg_param_value_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg,
			name, value, MAV_PARAM_TYPE_REAL32, parametersCount(), m.param_index);
		sendMessage(&msg);
	}

	if (msg.msgid == MAVLINK_MSG_ID_PARAM_SET) {
		mavlink_param_set_t m;
		mavlink_msg_param_set_decode(&msg, &m);
		if (m.target_system && m.target_system != SYSTEM_ID) return;

		char name[MAVLINK_MSG_PARAM_SET_FIELD_PARAM_ID_LEN + 1];
		strlcpy(name, m.param_id, sizeof(name)); // param_id might be not null-terminated
		setParameter(name, m.param_value);
		// send ack
		mavlink_message_t msg;
		mavlink_msg_param_value_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg,
			m.param_id, m.param_value, MAV_PARAM_TYPE_REAL32, parametersCount(), 0); // index is unknown
		sendMessage(&msg);
	}

	if (msg.msgid == MAVLINK_MSG_ID_MISSION_REQUEST_LIST) { // handle to make qgc happy
		mavlink_mission_request_list_t m;
		mavlink_msg_mission_request_list_decode(&msg, &m);
		if (m.target_system && m.target_system != SYSTEM_ID) return;

		mavlink_message_t msg;
		mavlink_msg_mission_count_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg, 0, 0, 0, MAV_MISSION_TYPE_MISSION, 0);
		sendMessage(&msg);
	}

	if (msg.msgid == MAVLINK_MSG_ID_SERIAL_CONTROL) {
		mavlink_serial_control_t m;
		mavlink_msg_serial_control_decode(&msg, &m);
		if (m.target_system && m.target_system != SYSTEM_ID) return;

		char data[MAVLINK_MSG_SERIAL_CONTROL_FIELD_DATA_LEN + 1];
		strlcpy(data, (const char *)m.data, m.count); // data might be not null-terminated
		doCommand(data, true);
	}

	if (msg.msgid == MAVLINK_MSG_ID_SET_ATTITUDE_TARGET) {
		if (mode != AUTO) return;

		mavlink_set_attitude_target_t m;
		mavlink_msg_set_attitude_target_decode(&msg, &m);
		if (m.target_system && m.target_system != SYSTEM_ID) return;

		// copy attitude, rates and thrust targets
		ratesTarget.x = m.body_roll_rate;
		ratesTarget.y = -m.body_pitch_rate; // convert to flu
		ratesTarget.z = -m.body_yaw_rate;
		attitudeTarget.w = m.q[0];
		attitudeTarget.x = m.q[1];
		attitudeTarget.y = -m.q[2];
		attitudeTarget.z = -m.q[3];
		thrustTarget = m.thrust;
		ratesExtra = Vector(0, 0, 0);

		if (m.type_mask & ATTITUDE_TARGET_TYPEMASK_ATTITUDE_IGNORE) attitudeTarget.invalidate();
		armed = m.thrust > 0;
	}

	if (msg.msgid == MAVLINK_MSG_ID_SET_ACTUATOR_CONTROL_TARGET) {
		if (mode != AUTO) return;

		mavlink_set_actuator_control_target_t m;
		mavlink_msg_set_actuator_control_target_decode(&msg, &m);
		if (m.target_system && m.target_system != SYSTEM_ID) return;

		attitudeTarget.invalidate();
		ratesTarget.invalidate();
		torqueTarget.invalidate();
		memcpy(motors, m.controls, sizeof(motors)); // copy motor thrusts
		armed = motors[0] > 0 || motors[1] > 0 || motors[2] > 0 || motors[3] > 0;
	}

	// Handle commands
	if (msg.msgid == MAVLINK_MSG_ID_COMMAND_LONG) {
		mavlink_command_long_t m;
		mavlink_msg_command_long_decode(&msg, &m);
		if (m.target_system && m.target_system != SYSTEM_ID) return;
		mavlink_message_t response;
		bool accepted = false;

		if (m.command == MAV_CMD_REQUEST_MESSAGE && m.param1 == MAVLINK_MSG_ID_AUTOPILOT_VERSION) {
			accepted = true;
			mavlink_msg_autopilot_version_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &response,
				MAV_PROTOCOL_CAPABILITY_PARAM_FLOAT | MAV_PROTOCOL_CAPABILITY_MAVLINK2, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0);
			sendMessage(&response);
		}

		if (m.command == MAV_CMD_COMPONENT_ARM_DISARM) {
			if (m.param1 && controlThrottle > 0.05) return; // don't arm if throttle is not low
			accepted = true;
			armed = m.param1 == 1;
		}

		if (m.command == MAV_CMD_DO_SET_MODE) {
			if (m.param2 < 0 || m.param2 > AUTO) return; // incorrect mode
			accepted = true;
			mode = m.param2;
		}

		// send command ack
		mavlink_message_t ack;
		mavlink_msg_command_ack_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &ack, m.command, accepted ? MAV_RESULT_ACCEPTED : MAV_RESULT_UNSUPPORTED, UINT8_MAX, 0, msg.sysid, msg.compid);
		sendMessage(&ack);
	}
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
		mavlink_msg_serial_control_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg,
			SERIAL_CONTROL_DEV_SHELL,
			i + MAVLINK_MSG_SERIAL_CONTROL_FIELD_DATA_LEN < strlen(str) ? SERIAL_CONTROL_FLAG_MULTI : 0, // more chunks to go
			0, 0, strlen(data), (uint8_t *)data, 0, 0);
		sendMessage(&msg);
	}
	mavlinkPrintBuffer.clear();
}

#endif
