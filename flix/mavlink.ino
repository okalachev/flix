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

		mavlink_msg_heartbeat_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg, MAV_TYPE_QUADROTOR,
			MAV_AUTOPILOT_GENERIC, MAV_MODE_FLAG_MANUAL_INPUT_ENABLED | (armed ? MAV_MODE_FLAG_SAFETY_ARMED : 0),
			0, MAV_STATE_STANDBY);
		sendMessage(&msg);
	}

	if (t - lastFast >= PERIOD_FAST) {
		lastFast = t;

		const float zeroQuat[] = {0, 0, 0, 0};
		mavlink_msg_attitude_quaternion_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg,
			time, attitude.w, attitude.x, attitude.y, attitude.z, rates.x, rates.y, rates.z, zeroQuat);
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
		controls[RC_CHANNEL_THROTTLE] = manualControl.z / 1000.0f;
		controls[RC_CHANNEL_PITCH] = manualControl.x / 1000.0f * MAVLINK_CONTROL_SCALE;
		controls[RC_CHANNEL_ROLL] = manualControl.y / 1000.0f * MAVLINK_CONTROL_SCALE;
		controls[RC_CHANNEL_YAW] = manualControl.r / 1000.0f * MAVLINK_CONTROL_SCALE;
		controls[RC_CHANNEL_MODE] = 1; // STAB mode
		controls[RC_CHANNEL_ARMED] = 1; // armed

		if (abs(controls[RC_CHANNEL_YAW]) < MAVLINK_CONTROL_YAW_DEAD_ZONE) controls[RC_CHANNEL_YAW] = 0;
	}
}

#endif
