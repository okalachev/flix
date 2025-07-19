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

extern float controlTime;

void processMavlink() {
	sendMavlink();
	receiveMavlink();
}

void sendMavlink() {
	static float lastSlow = 0;
	static float lastFast = 0;

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
		mavlink_msg_attitude_quaternion_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg,
			time, attitude.w, attitude.x, -attitude.y, -attitude.z, rates.x, -rates.y, -rates.z, zeroQuat); // convert to frd
		sendMessage(&msg);

		mavlink_msg_rc_channels_raw_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg, controlTime * 1000, 0,
			channels[0], channels[1], channels[2], channels[3], channels[4], channels[5], channels[6], channels[7], UINT8_MAX);
		sendMessage(&msg);

		float actuator[32];
		memcpy(actuator, motors, sizeof(motors));
		mavlink_msg_actuator_output_status_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg, time, 4, actuator);
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
		controlThrottle = m.z / 1000.0f;
		controlPitch = m.x / 1000.0f * MAVLINK_CONTROL_SCALE;
		controlRoll = m.y / 1000.0f * MAVLINK_CONTROL_SCALE;
		controlYaw = m.r / 1000.0f * MAVLINK_CONTROL_SCALE;
		controlMode = 1; // STAB mode
		controlArmed = 1; // armed
		controlTime = t;

		if (abs(controlYaw) < MAVLINK_CONTROL_YAW_DEAD_ZONE) controlYaw = 0;
	}
}

#endif
