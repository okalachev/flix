// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Wi-Fi and ESP-NOW communication

#include <WiFi.h>
#include <WiFiAP.h>
#include <WiFiUdp.h>
#include <MacAddress.h>
#include <ESP32_NOW_Serial.h>
#include "Preferences.h"
#include "util.h"

extern Preferences storage; // use the main preferences storage

const int W_DISABLED = 0, W_AP = 1, W_STA = 2, W_ESPNOW = 3;
int wifiMode = W_AP;

int wifiLongRange = 0;
int udpLocalPort = 14550;
int udpRemotePort = 14550;
IPAddress udpRemoteIP = "255.255.255.255";
WiFiUDP udp;

ESPNOWSerial espnow(NULL, 0, WIFI_IF_AP);
ESPNOWSerial espnowBroadcast(ESP_NOW.BROADCAST_ADDR, 0, WIFI_IF_AP);
int espnowChannel = 6;

void setupWiFi() {
	print("Setup Wi-Fi\n");
	WiFi.enableLongRange(wifiLongRange);

	if (wifiMode == W_AP) {
		WiFi.softAP(storage.getString("WIFI_AP_SSID", "flix").c_str(), storage.getString("WIFI_AP_PASS", "flixwifi").c_str());
		udp.begin(udpLocalPort);
	} else if (wifiMode == W_STA) {
		WiFi.begin(storage.getString("WIFI_STA_SSID", "").c_str(), storage.getString("WIFI_STA_PASS", "").c_str());
		udp.begin(udpLocalPort);
	} else if (wifiMode == W_ESPNOW) {
		WiFi.mode(WIFI_AP);
		WiFi.setChannel(espnowChannel);
		espnow.addr(MacAddress(storage.getString("ESPNOW_PEER_MAC", "FF:FF:FF:FF:FF:FF").c_str()));
		String key = storage.getString("ESPNOW_PEER_KEY", "");
		espnow.setKey(key.isEmpty() ? nullptr : (const uint8_t *)key.c_str());
		espnow.begin();
		espnowBroadcast.begin();
	}

	WiFi.setSleep(false); // disable power save
}

void sendWiFi(const uint8_t *buf, int len) {
	if (espnow) {
		espnow.write(buf, len);
		static Rate discovery(2);
		if (discovery) espnowBroadcast.write((const uint8_t *)"flix", 4); // broadcast message to help finding this device
		return;
	}

	if (WiFi.softAPgetStationNum() == 0 && !WiFi.isConnected()) return;

	udp.beginPacket(udpRemoteIP, udpRemotePort);
	udp.write(buf, len);
	udp.endPacket();
}

int receiveWiFi(uint8_t *buf, int len) {
	if (espnow) {
		return espnow.read(buf, len);
	}

	if (WiFi.softAPgetStationNum() == 0 && !WiFi.isConnected()) return 0;

	udp.parsePacket();
	if (udp.remoteIP()) udpRemoteIP = udp.remoteIP();
	return udp.read(buf, len);
}

void printWiFiInfo() {
	if (espnow) {
		print("Mode: ESP-NOW\n");
		print("ESP-NOW version: %d\n", ESP_NOW.getVersion());
		print("Max packet size: %d\n", ESP_NOW.getMaxDataLen());
		print("MAC: %s\n", WiFi.softAPmacAddress().c_str());
		print("Peer MAC: %s\n", MacAddress(espnow.addr()).toString().c_str());
		print("Encrypted: %d\n", espnow.isEncrypted());
		print("Channel: %d\n", espnow.getChannel());
	} else if (WiFi.getMode() == WIFI_MODE_AP) {
		print("Mode: Access Point (AP)\n");
		print("MAC: %s\n", WiFi.softAPmacAddress().c_str());
		print("SSID: %s\n", WiFi.softAPSSID().c_str());
		print("Password: ***\n");
		print("Channel: %d\n", WiFi.channel());
		print("Clients: %d\n", WiFi.softAPgetStationNum());
		print("IP: %s\n", WiFi.softAPIP().toString().c_str());
		print("Remote IP: %s\n", udpRemoteIP.toString().c_str());
	} else if (WiFi.getMode() == WIFI_MODE_STA) {
		print("Mode: Client (STA)\n");
		print("Connected: %d\n", WiFi.isConnected());
		print("MAC: %s\n", WiFi.macAddress().c_str());
		print("SSID: %s\n", WiFi.SSID().c_str());
		print("Password: ***\n");
		print("Channel: %d\n", WiFi.channel());
		print("RSSI: %d dBm\n", WiFi.RSSI());
		print("IP: %s\n", WiFi.localIP().toString().c_str());
		print("Remote IP: %s\n", udpRemoteIP.toString().c_str());
	} else {
		print("Mode: Disabled\n");
	}
	print("MAVLink connected: %d\n", mavlinkConnected);
}

void configWiFi(int mode, const char *first, const char *second) {
	MacAddress mac;
	if (mode == W_AP && strlen(first) > 0 && strlen(second) >= 8) {
		storage.putString("WIFI_AP_SSID", first);
		storage.putString("WIFI_AP_PASS", second);
	} else if (mode == W_STA && strlen(first) > 0 && strlen(second) >= 8) {
		storage.putString("WIFI_STA_SSID", first);
		storage.putString("WIFI_STA_PASS", second);
	} else if (mode == W_ESPNOW && mac.fromString(first)) {
		storage.putString("ESPNOW_PEER_MAC", first);
		storage.putString("ESPNOW_PEER_KEY", strlen(second) == ESP_NOW_KEY_LEN ? second : "");
	} else {
		print("Invalid configuration\n");
		return;
	}
	print("✓ Reboot to apply new settings\n");
}
