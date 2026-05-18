// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Wi-Fi and ESP-NOW communication

#include <WiFi.h>
#include <WiFiAP.h>
#include <WiFiUdp.h>
#include <MacAddress.h>
#include <ESP32_NOW_Serial.h>
#include "Preferences.h"

extern Preferences storage; // use the main preferences storage

const int W_DISABLED = 0, W_AP = 1, W_STA = 2, W_ESPNOW = 3;
int wifiMode = W_AP;
int wifiLongRange = 0;
int udpLocalPort = 14550;
int udpRemotePort = 14550;
int espnowChannel = 6;
IPAddress udpRemoteIP = "255.255.255.255";

WiFiUDP udp;
ESP_NOW_Serial_Class espnow(NULL, 0, WIFI_IF_AP);

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
		espnow.setChannel(espnowChannel);
		espnow.addr(MacAddress(storage.getString("ESPNOW_PEER_MAC", "").c_str()));
		espnow.begin();
	}

	WiFi.setSleep(false); // disable power save
}

void sendWiFi(const uint8_t *buf, int len) {
	if (espnow) {
		espnow.write(buf, len);
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
	if (mode == W_AP) {
		storage.putString("WIFI_AP_SSID", first);
		storage.putString("WIFI_AP_PASS", second);
	} else if (mode == W_STA) {
		storage.putString("WIFI_STA_SSID", first);
		storage.putString("WIFI_STA_PASS", second);
	} else if (mode == W_ESPNOW) {
		storage.putString("ESPNOW_PEER_MAC", first);
	}
	print("✓ Reboot to apply new settings\n");
}
