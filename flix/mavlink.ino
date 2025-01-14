// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// MAVLink communication

#if WIFI_ENABLED == 1

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

		mavlink_msg_heartbeat_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg, MAV_TYPE_QUADROTOR,
			MAV_AUTOPILOT_GENERIC, MAV_MODE_FLAG_MANUAL_INPUT_ENABLED | (armed ? MAV_MODE_FLAG_SAFETY_ARMED : 0),
			0, MAV_STATE_STANDBY);
		sendMessage(&msg);
	}

	if (t - lastFast >= PERIOD_FAST) {
		lastFast = t;

		const float zeroQuat[] = {0, 0, 0, 0};
		Quaternion attitudeFRD = FLU2FRD(attitude); // MAVLink uses FRD coordinate system
		mavlink_msg_attitude_quaternion_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg,
			time, attitudeFRD.w, attitudeFRD.x, attitudeFRD.y, attitudeFRD.z, rates.x, rates.y, rates.z, zeroQuat);
		sendMessage(&msg);

		mavlink_msg_rc_channels_scaled_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg, time, 0,
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
	mavlink_message_t *msg = (mavlink_message_t *)_msg;

	if (msg->msgid == MAVLINK_MSG_ID_MANUAL_CONTROL) {
		mavlink_manual_control_t manualControl;
		mavlink_msg_manual_control_decode(msg, &manualControl);
		controls[throttleChannel] = manualControl.z / 1000.0f;
		controls[pitchChannel] = manualControl.x / 1000.0f * mavlinkControlScale;
		controls[rollChannel] = manualControl.y / 1000.0f * mavlinkControlScale;
		controls[yawChannel] = manualControl.r / 1000.0f * mavlinkControlScale;
		controls[modeChannel] = 1; // STAB mode
		controls[armedChannel] = 1; // armed
		controlsTime = t;

		if (abs(controls[yawChannel]) < MAVLINK_CONTROL_YAW_DEAD_ZONE) controls[yawChannel] = 0;
	}

	if (msg->msgid == MAVLINK_MSG_ID_PARAM_REQUEST_LIST) {
		mavlink_message_t msg;
		for (int i = 0; i < parametersCount(); i++) {
			mavlink_msg_param_value_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg,
				getParameterName(i), getParameter(i), MAV_PARAM_TYPE_REAL32, parametersCount(), i);
			sendMessage(&msg);
		}
	}

	if (msg->msgid == MAVLINK_MSG_ID_PARAM_REQUEST_READ) {
		mavlink_param_request_read_t paramRequestRead;
		mavlink_msg_param_request_read_decode(msg, &paramRequestRead);
		char name[16 + 1];
		strlcpy(name, paramRequestRead.param_id, sizeof(name)); // param_id might be not null-terminated
		float value = strlen(name) == 0 ? getParameter(paramRequestRead.param_index) : getParameter(name);
		if (paramRequestRead.param_index != -1) {
			memcpy(name, getParameterName(paramRequestRead.param_index), 16);
		}
		mavlink_message_t msg;
		mavlink_msg_param_value_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg,
			name, value, MAV_PARAM_TYPE_REAL32, parametersCount(), paramRequestRead.param_index);
		sendMessage(&msg);
	}

	if (msg->msgid == MAVLINK_MSG_ID_PARAM_SET) {
		mavlink_param_set_t paramSet;
		mavlink_msg_param_set_decode(msg, &paramSet);
		char name[16 + 1];
		strlcpy(name, paramSet.param_id, sizeof(name)); // param_id might be not null-terminated
		setParameter(name, paramSet.param_value);
		// send ack
		mavlink_message_t msg;
		mavlink_msg_param_value_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg,
			paramSet.param_id, paramSet.param_value, MAV_PARAM_TYPE_REAL32, parametersCount(), 0); // index is unknown
		sendMessage(&msg);
	}

	if (msg->msgid == MAVLINK_MSG_ID_MISSION_REQUEST_LIST) { // handle to make qgc happy
		mavlink_message_t msg;
		mavlink_msg_mission_count_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg, 0, 0, 0, MAV_MISSION_TYPE_MISSION, 0);
		sendMessage(&msg);
	}

	// Handle commands
	if (msg->msgid == MAVLINK_MSG_ID_COMMAND_LONG) {
		mavlink_command_long_t commandLong;
		mavlink_msg_command_long_decode(msg, &commandLong);
		mavlink_message_t ack;
		mavlink_message_t response;

		if (commandLong.command == MAV_CMD_REQUEST_MESSAGE && commandLong.param1 == MAVLINK_MSG_ID_AUTOPILOT_VERSION) {
			mavlink_msg_command_ack_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &ack, commandLong.command, MAV_RESULT_ACCEPTED, UINT8_MAX, 0, msg->sysid, msg->compid);
			sendMessage(&ack);
			mavlink_msg_autopilot_version_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &response,
				MAV_PROTOCOL_CAPABILITY_PARAM_FLOAT | MAV_PROTOCOL_CAPABILITY_MAVLINK2, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0);
			sendMessage(&response);
		} else {
			mavlink_msg_command_ack_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &ack, commandLong.command, MAV_RESULT_UNSUPPORTED, UINT8_MAX, 0, msg->sysid, msg->compid);
			sendMessage(&ack);
		}
	}
}

// Convert Forward-Left-Up to Forward-Right-Down quaternion
inline Quaternion FLU2FRD(const Quaternion &q) {
	return Quaternion(q.w, q.x, -q.y, -q.z);
}

#endif
