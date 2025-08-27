// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Wi-Fi support

#if WIFI_ENABLED

#include <WiFi.h>
#include <WiFiAP.h>
#include <WiFiUdp.h>

#define WIFI_SSID "flix"
#define WIFI_PASSWORD "flixwifi"
#define WIFI_UDP_PORT 14550
#define WIFI_UDP_REMOTE_PORT 14550
#define WIFI_UDP_REMOTE_ADDR "255.255.255.255"

WiFiUDP udp;

void setupWiFi() {
	print("Setup Wi-Fi\n");
	WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);
	udp.begin(WIFI_UDP_PORT);
}

void sendWiFi(const uint8_t *buf, int len) {
	if (WiFi.softAPIP() == IPAddress(0, 0, 0, 0) && WiFi.status() != WL_CONNECTED) return;
	udp.beginPacket(udp.remoteIP() ? udp.remoteIP() : WIFI_UDP_REMOTE_ADDR, WIFI_UDP_REMOTE_PORT);
	udp.write(buf, len);
	udp.endPacket();
}

int receiveWiFi(uint8_t *buf, int len) {
	udp.parsePacket();
	return udp.read(buf, len);
}

#endif
