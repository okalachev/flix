// Copyright (c) 2026 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Proxy for ESP-NOW connection

#include <vector>
#include <WiFi.h>
#include <ESP32_NOW_Serial.h>
#include <MacAddress.h>
#include <MAVLink.h>
#include <Preferences.h>
#include <freertos/semphr.h>
#include <CRSF.h>
#include "../../flix/util.h"

#ifndef LED_BUILTIN
#define LED_BUILTIN 2 // most common LED pin for ESP32 boards
#endif

Preferences storage;
String key; // may be empty
String discoveryString;
int channel;
bool broadcast = false; // broadcast mode, use only one broadcast peer
std::vector<ESPNOWSerial *> peers; // list of peers to send data to
ESPNOWSerial *broadcastPeer;
xSemaphoreHandle peersMutex;
int crsfRxPin = 26;

CRSF crsf(Serial1);

void onNewPeer(const esp_now_recv_info_t *info, const uint8_t *data, int len, void *arg) {
	if (len != discoveryString.length() || memcmp(data, discoveryString.c_str(), discoveryString.length()) != 0) {
		return; // not a discovery message
	}

	Serial.printf("New peer: " MACSTR "\n", MAC2STR(info->src_addr));
	ESPNOWSerial *link = new ESPNOWSerial(info->src_addr, channel, WIFI_IF_STA, (const uint8_t *)key.c_str());
	link->begin();

	xSemaphoreTake(peersMutex, portMAX_DELAY);
	peers.push_back(link);
	xSemaphoreGive(peersMutex);

	digitalWrite(LED_BUILTIN, HIGH);
}

void setup() {
	peersMutex = xSemaphoreCreateMutex();
	storage.begin("espnow-proxy");

	pinMode(LED_BUILTIN, OUTPUT);

	// Generate or retrieve the key
	if (!storage.isKey("key")) {
		generateRandomKey();
		storage.putString("key", key);
	}
	key = storage.getString("key");

	// Generate or retrieve the channel
	if (!storage.isKey("channel")) {
		channel = random(1, 14); // WiFi channels 1-13
		storage.putInt("channel", channel);
	}
	channel = storage.getInt("channel");

	Serial.begin(115200);
	WiFi.mode(WIFI_STA);
	WiFi.setSleep(false);
	WiFi.setChannel(channel);
	WiFi.STA.begin(false);

	MacAddress mac = WiFi.macAddress();
	discoveryString = "flix " + mac.toString();

	if (broadcast) {
		ESPNOWSerial *peer = new ESPNOWSerial(ESP_NOW.BROADCAST_ADDR, channel, WIFI_IF_STA);
		peer->begin();
		peers.push_back(peer);
	}

	ESP_NOW.onNewPeer(onNewPeer, NULL);
	ESP_NOW.begin();

	crsf.begin(crsfRxPin, -1, true, 400000);

	// Print setup command until at least one peer is available
	while (peers.empty()) {
		Serial.printf("espnow %d %s %s\n", channel, mac.toString().c_str(), key.c_str());
		delay(500);
	}
}

void generateRandomKey() {
	key.clear();
	const char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#$%^&*-_+=";
	for (int i = 0; i < ESP_NOW_KEY_LEN; i++) {
		key += chars[random(0, strlen(chars))];
	}
}

void loop() {
	static uint8_t buf[5000];

	// Send from Serial to ESP-NOW
	while (Serial.available() > 0) {
		int b = Serial.read();
		if (b < 0) {
			break;
		}

		mavlink_message_t msg;
		mavlink_status_t status;
		if (mavlink_parse_char(MAVLINK_COMM_0, (uint8_t)b, &msg, &status)) {
			int len = mavlink_msg_to_send_buffer(buf, &msg);

			// Send to registered peers
			xSemaphoreTake(peersMutex, portMAX_DELAY);
			for (ESPNOWSerial *link : peers) {
				link->write(buf, len);
			}
			xSemaphoreGive(peersMutex);
		}
	}

	// Send from ESP-NOW to Serial
	for (ESPNOWSerial *link : peers) {
		int len = link->read(buf, sizeof(buf));
		if (len > 0) {
			Serial.write(buf, len);
		}
	}

	// Read channels from CRSF
	if (crsf.read()) {
		uint16_t channels[16];
		crsf.getChannels(channels);
		mavlink_message_t msg;
		mavlink_msg_rc_channels_override_pack(255, MAV_COMP_ID_UART_BRIDGE, &msg, 0, 0,
			channels[0], channels[1], channels[2], channels[3], channels[4], channels[5],
			channels[6], channels[7], channels[8], channels[9], channels[10], channels[11],
			channels[12], channels[13], channels[14], channels[15], 0, 0);
		int len = mavlink_msg_to_send_buffer(buf, &msg);
		for (ESPNOWSerial *link : peers) {
			link->write(buf, len);
		}
		Serial.write(buf, len);
	}
}
