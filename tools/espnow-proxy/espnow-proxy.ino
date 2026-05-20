// Copyright (c) 2026 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Proxy for ESP-NOW connection

#include <WiFi.h>
#include <ESP32_NOW_Serial.h>
#include <MacAddress.h>
#include <MAVLink.h>
#include <Preferences.h>
#include "../../flix/util.h"
#include <vector>

const int CHANNEL = 6;
char key[ESP_NOW_KEY_LEN + 1] = {0}; // with trailing null

Preferences storage;

std::vector<ESPNOWSerial *> peers;

void onNewPeer(const esp_now_recv_info_t *info, const uint8_t *data, int len, void *arg) {
	if (len != 4 || memcmp(data, "flix", 4) != 0) return; // check if discovery message

	Serial.printf("New peer: " MACSTR "\n", MAC2STR(info->src_addr));
	ESPNOWSerial *link = new ESPNOWSerial(info->src_addr, CHANNEL, WIFI_IF_AP);
	link->begin();
	link->setKey((const uint8_t *)key);
	peers.push_back(link);
}

void setup() {
	Serial.begin(115200);
	WiFi.mode(WIFI_AP);
	WiFi.setSleep(false);
	WiFi.setChannel(CHANNEL);

	ESP_NOW.onNewPeer(onNewPeer, NULL);
	ESP_NOW.begin();

	storage.begin("espnow-proxy");
	if (!storage.isKey("key")) {
		generateRandomKey();
		storage.putString("key", key);
	}
	strcpy(key, storage.getString("key").c_str());

	// Discover the first peer
	while (peers.empty()) {
		Serial.printf("espnow %s %s\n", WiFi.softAPmacAddress().c_str(), key);
		delay(500);
	}
}

void generateRandomKey() {
	const char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#$%^&*-_+=";
	for (int i = 0; i < ESP_NOW_KEY_LEN; i++) {
		key[i] = chars[random(0, strlen(chars))];
	}
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
			for (ESPNOWSerial *link : peers) {
				link->write(buf, len);
			}
		}
	}

	// Send from ESP-NOW to serial
	for (ESPNOWSerial *link : peers) {
		int len = link->read(buf, sizeof(buf));
		if (len > 0) {
			Serial.write(buf, len);
		}
	}
}
