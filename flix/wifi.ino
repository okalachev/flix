// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFi/examples/WiFiAccessPoint/WiFiAccessPoint.ino
// https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFi/examples/WiFiClient/WiFiClient.ino

#if WIFI_ENABLED == 1

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include "SBUS.h"
#include "mavlink/common/mavlink.h"

#define WIFI_SSID "flix"
#define WIFI_PASSWORD "flixwifi"
// #define WIFI_UDP_IP "192.168.4.255"
#define WIFI_UDP_IP "255.255.255.255"
// #define WIFI_UDP_IP "192.168.4.2"
#define WIFI_UDP_PORT 14550

WiFiUDP udp;

void setupWiFi()
{
	Serial.println("Setup Wi-Fi");
	WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);
	IPAddress myIP = WiFi.softAPIP();
}

inline void sendWiFi(const uint8_t *buf, size_t len)
{
	udp.beginPacket(WIFI_UDP_IP, WIFI_UDP_PORT);
	udp.write(buf, len);
	udp.endPacket();
}

#endif
