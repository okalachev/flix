// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Wi-Fi support

#if WIFI_ENABLED == 1

#include <WiFi.h>
#include <WiFiAP.h>
#include <WiFiUdp.h>

#define WIFI_SSID "flix"
#define WIFI_PASSWORD "flixwifi"
#define WIFI_UDP_IP "255.255.255.255"
#define WIFI_UDP_PORT 14550

WiFiUDP udp;

void setupWiFi() {
	Serial.println("Setup Wi-Fi");
	WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);
	IPAddress myIP = WiFi.softAPIP();
	udp.begin(WIFI_UDP_PORT);
}

void sendWiFi(const uint8_t *buf, int len) {
	udp.beginPacket(WIFI_UDP_IP, WIFI_UDP_PORT);
	udp.write(buf, len);
	udp.endPacket();
}

int receiveWiFi(uint8_t *buf, int len) {
	udp.parsePacket();
	return udp.read(buf, len);
}

#endif
