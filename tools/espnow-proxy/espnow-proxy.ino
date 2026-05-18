// Copyright (c) 2026 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Proxy for ESP-NOW connection

#include <WiFi.h>
#include <ESP32_NOW_Serial.h>
#include <MacAddress.h>
#include <MAVLink.h>

const int CHANNEL = 6;

ESP_NOW_Serial_Class espnow(NULL, CHANNEL, WIFI_IF_AP);
MacAddress peerMac;
volatile bool peerFound = false;

void onNewPeer(const esp_now_recv_info_t *info, const uint8_t *data, int len, void *arg) {
	peerMac = info->src_addr;
	peerFound = true;
}

void setup() {
	Serial.begin(115200);
	WiFi.mode(WIFI_AP);
	WiFi.setSleep(false);
	WiFi.setChannel(CHANNEL);

	// while (!WiFi.AP.started()) {
		// delay(100);
	// }

	ESP_NOW.onNewPeer(onNewPeer, NULL);
	ESP_NOW.begin();

	while (!peerFound) {
		Serial.printf("MAC: %s, waiting for peer...\n", WiFi.softAPmacAddress().c_str());
		delay(1000);
	}
	Serial.printf("Peer found: %s\n", peerMac.toString().c_str());

	espnow.addr(peerMac);
	espnow.setChannel(CHANNEL);
	espnow.begin();
}

void loop() {
	uint8_t buf[5000];

	// Send from serial to ESP-NOW
	while (Serial.available() > 0) {
		int b = Serial.read();
		if (b < 0) {
			break;
		}

		mavlink_message_t msg;
		mavlink_status_t status;
		if (mavlink_parse_char(MAVLINK_COMM_0, (uint8_t)b, &msg, &status)) {
			int len = mavlink_msg_to_send_buffer(buf, &msg);
			// ESP_NOW.write(buf, len);
			espnow.write(buf, len);
			// espnow.send(buf, len);
			// esp_now_send(peerMac, buf, len);
		}
	}

	// Send from ESP-NOW to serial
	int len = espnow.read(buf, sizeof(buf));
	if (len > 0) {
		Serial.write(buf, len);
	}
}
