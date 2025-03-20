// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// MAVLink communication

#if WIFI_ENABLED

#include <MAVLink.h>

#define SYSTEM_ID 1
#define PERIOD_SLOW 1.0
#define PERIOD_FAST 0.1
#define MAVLINK_CONTROL_SCALE 0.7f
#define MAVLINK_CONTROL_YAW_DEAD_ZONE 0.1f

float mavlinkControlScale = 0.7;

extern double controlsTime;
extern int rollChannel, pitchChannel, throttleChannel, yawChannel, armedChannel, modeChannel;

void processMavlink() {
	sendMavlink();
	receiveMavlink();
}

void sendMavlink() {
	static double lastSlow = 0;
	static double lastFast = 0;

	mavlink_message_t msg;
	uint32_t time = t * 1000;

	if (t - lastSlow >= PERIOD_SLOW) {
		lastSlow = t;

		mavlink_msg_heartbeat_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg, MAV_TYPE_QUADROTOR, MAV_AUTOPILOT_GENERIC,
			MAV_MODE_FLAG_MANUAL_INPUT_ENABLED | (armed * MAV_MODE_FLAG_SAFETY_ARMED) | ((mode == STAB) * MAV_MODE_FLAG_STABILIZE_ENABLED),
			0, MAV_STATE_STANDBY);
		sendMessage(&msg);
	}

	if (t - lastFast >= PERIOD_FAST) {
		lastFast = t;

		const float zeroQuat[] = {0, 0, 0, 0};
		Quaternion attitudeFRD = fluToFrd(attitude); // MAVLink uses FRD coordinate system
		mavlink_msg_attitude_quaternion_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg,
			time, attitudeFRD.w, attitudeFRD.x, attitudeFRD.y, attitudeFRD.z, rates.x, rates.y, rates.z, zeroQuat);
		sendMessage(&msg);

		mavlink_msg_rc_channels_scaled_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg, controlsTime * 1000, 0,
			controls[0] * 10000, controls[1] * 10000, controls[2] * 10000,
			controls[3] * 10000, controls[4] * 10000, controls[5] * 10000,
			INT16_MAX, INT16_MAX, UINT8_MAX);
		sendMessage(&msg);

		float actuator[32];
		memcpy(actuator, motors, sizeof(motors));
		mavlink_msg_actuator_output_status_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg, time, 4, actuator);
		sendMessage(&msg);

		mavlink_msg_scaled_imu_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg, time,
			acc.x * 1000, acc.y * 1000, acc.z * 1000,
			gyro.x * 1000, gyro.y * 1000, gyro.z * 1000,
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

		controls[throttleChannel] = m.z / 1000.0f;
		controls[pitchChannel] = m.x / 1000.0f * mavlinkControlScale;
		controls[rollChannel] = m.y / 1000.0f * mavlinkControlScale;
		controls[yawChannel] = m.r / 1000.0f * mavlinkControlScale;
		controls[modeChannel] = 1; // STAB mode
		controls[armedChannel] = 1; // armed
		controlsTime = t;

		if (abs(controls[yawChannel]) < MAVLINK_CONTROL_YAW_DEAD_ZONE) controls[yawChannel] = 0;
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

	// Handle commands
	if (msg.msgid == MAVLINK_MSG_ID_COMMAND_LONG) {
		mavlink_command_long_t m;
		mavlink_msg_command_long_decode(&msg, &m);
		if (m.target_system && m.target_system != SYSTEM_ID) return;
		mavlink_message_t ack;
		mavlink_message_t response;

		if (m.command == MAV_CMD_REQUEST_MESSAGE && m.param1 == MAVLINK_MSG_ID_AUTOPILOT_VERSION) {
			mavlink_msg_command_ack_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &ack, m.command, MAV_RESULT_ACCEPTED, UINT8_MAX, 0, msg.sysid, msg.compid);
			sendMessage(&ack);
			mavlink_msg_autopilot_version_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &response,
				MAV_PROTOCOL_CAPABILITY_PARAM_FLOAT | MAV_PROTOCOL_CAPABILITY_MAVLINK2, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0);
			sendMessage(&response);
		} else {
			mavlink_msg_command_ack_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &ack, m.command, MAV_RESULT_UNSUPPORTED, UINT8_MAX, 0, msg.sysid, msg.compid);
			sendMessage(&ack);
		}
	}
}

// Send shell output to GCS
void mavlinkPrint(const char* str) {
	// Send data in chunks
	for (int i = 0; i < strlen(str); i += MAVLINK_MSG_SERIAL_CONTROL_FIELD_DATA_LEN) {
		char data[MAVLINK_MSG_SERIAL_CONTROL_FIELD_DATA_LEN + 1];
		strlcpy(data, str + i, sizeof(data));
		mavlink_message_t msg;
		mavlink_msg_serial_control_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg,
			SERIAL_CONTROL_DEV_SHELL, 0, 0, 0, strlen(data), (uint8_t *)data, 0, 0);
		sendMessage(&msg);
	}
}

// Convert Forward-Left-Up to Forward-Right-Down quaternion
inline Quaternion fluToFrd(const Quaternion &q) {
	return Quaternion(q.w, q.x, -q.y, -q.z);
}

#endif
