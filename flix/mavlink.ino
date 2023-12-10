// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

#if WIFI_ENABLED == 1

#include "mavlink/common/mavlink.h"

#define SYSTEM_ID 1
#define PERIOD_SLOW 1.0
#define PERIOD_FAST 0.1

void sendMavlink()
{
	static float lastSlow = 0;
	static float lastFast = 0;

	mavlink_message_t msg;

	if (t - lastSlow >= PERIOD_SLOW) {
		lastSlow = t;

		mavlink_msg_heartbeat_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg, MAV_TYPE_QUADROTOR,
			MAV_AUTOPILOT_GENERIC, MAV_MODE_FLAG_MANUAL_INPUT_ENABLED | MAV_MODE_FLAG_SAFETY_ARMED,
			0, MAV_STATE_STANDBY);
		sendMessage(&msg);

		// params test
		// mavlink_msg_param_value_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg, "PITCHRATE_P", PITCHRATE_D, MAV_PARAM_TYPE_REAL32, 3, 0);
		// sendMessage(&msg);
		// mavlink_msg_param_value_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg, "PITCHRATE_I", PITCHRATE_I, MAV_PARAM_TYPE_REAL32, 3, 1);
		// sendMessage(&msg);
		// mavlink_msg_param_value_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg, "PITCHRATE_D", PITCHRATE_D, MAV_PARAM_TYPE_REAL32, 3, 2);
		// sendMessage(&msg);
	}

	if (t - lastFast >= PERIOD_FAST) {
		lastFast = t;

		// mavlink_msg_attitude_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg, t / 1000, NAN, NAN, NAN, rollRate, pitchRate, yawRate);
		// sendMessage(&msg);

		const float zeroQuat[] = {0, 0, 0, 0};

		mavlink_msg_attitude_quaternion_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg,
			t / 1000, attitude.w, attitude.x, attitude.y, attitude.z, rates.x, rates.y, rates.z, zeroQuat);
		// mavlink_msg_attitude_quaternion_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg,
		// 	t / 1000, attitudeTarget.w, attitudeTarget.x, attitudeTarget.y, attitudeTarget.z, rates.x, rates.y, rates.z, zeroQuat);
		sendMessage(&msg);

		mavlink_msg_rc_channels_scaled_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg, t / 1000, 0,
			controls[0] * 10000, controls[1] * 10000, controls[2] * 10000,
			controls[3] * 10000, controls[4] * 10000, controls[5] * 10000,
			UINT16_MAX, UINT16_MAX, 255);
		sendMessage(&msg);

		float actuator[32];
		memcpy(motors, actuator, 4 * sizeof(float));
		mavlink_msg_actuator_output_status_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg, t / 1000, 4, actuator);
		sendMessage(&msg);
	}
}

inline void sendMessage(const void *msg)
{
	uint8_t buf[MAVLINK_MAX_PACKET_LEN];
	uint16_t len = mavlink_msg_to_send_buffer(buf, (mavlink_message_t *)msg);
	sendWiFi(buf, len);
}

#endif
