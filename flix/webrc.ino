// Copyright (c) 2026 Pavel Arkhipov
// Repository: https://github.com/okalachev/flix

// Web remote control: phone/browser joystick page + telemetry + console
// Served at http://192.168.4.1/ while the drone runs as a Wi-Fi access point.
//
// The HTTP server runs in its own FreeRTOS task on core 0, so slow clients never stall
// the flight loop (core 1). The task only stores the latest stick values and queued
// commands; loopWebRC() applies them inside the main loop.

#include <WiFi.h>
#include <WebServer.h>
#include "quaternion.h"
#include "util.h"
#include "webrc_page.h"

extern float t, loopRate, voltage, voltageLow;
extern float controlRoll, controlPitch, controlYaw, controlThrottle, controlMode, controlTime;
extern float motors[4];
extern Quaternion attitude;
extern int mode;
extern bool armed;
extern const int RAW, ACRO, STAB, AUTO;
extern const int W_AP, W_STA;
extern int wifiMode;
extern bool batteryLow;

static WebServer webServer(80);

// Latest sticks from the page, written by the web task, read by the main loop
static volatile float webRoll, webPitch, webYaw, webThrottle;
static volatile uint32_t webSeq; // incremented on every stick update
static volatile int webRequest = 0; // 1 - arm, 2 - disarm
static volatile int webMode = -1; // requested flight mode
static String webCommand; // console command waiting for the main loop
static SemaphoreHandle_t webLock;

// Console output ring for the page (filled by print())
static const int WEB_LOG_SIZE = 4096;
static char webLog[WEB_LOG_SIZE];
static uint32_t webLogHead = 0; // total bytes ever written

void webrcPrint(const char *str) {
	if (!webLock) return;
	xSemaphoreTake(webLock, portMAX_DELAY);
	for (const char *c = str; *c; c++) webLog[webLogHead++ % WEB_LOG_SIZE] = *c;
	xSemaphoreGive(webLock);
}

static float argFloat(const char *name, float def) {
	if (!webServer.hasArg(name)) return def;
	return constrain(webServer.arg(name).toFloat(), -1.0f, 1.0f);
}

static const char *modeName(int m) {
	if (m == RAW) return "RAW";
	if (m == ACRO) return "ACRO";
	if (m == STAB) return "STAB";
	if (m == AUTO) return "AUTO";
	return "?";
}

static float fin(float x) { return isfinite(x) ? x : 0; } // JSON has no nan

static void sendStatus() {
	Vector e = attitude.toEuler();
	e = Vector(fin(e.x), fin(e.y), fin(e.z));
	char buf[360];
	snprintf(buf, sizeof(buf),
		"{\"armed\":%d,\"mode\":\"%s\",\"v\":%.2f,\"roll\":%.1f,\"pitch\":%.1f,\"yaw\":%.1f,"
		"\"thr\":%.2f,\"mot\":[%.2f,%.2f,%.2f,%.2f],\"hz\":%.0f,\"t\":%.1f,\"low\":%d,\"vlow\":%.2f}",
		armed, modeName(mode), fin(voltage), degrees(e.x), degrees(e.y), degrees(e.z),
		fin(controlThrottle), fin(motors[0]), fin(motors[1]), fin(motors[2]), fin(motors[3]), fin(loopRate), fin(t), batteryLow, voltageLow);
	webServer.send(200, "application/json", buf);
}

static void handleRC() {
	// r, p, y in [-1, 1], th in [0, 1]
	webRoll = argFloat("r", 0);
	webPitch = argFloat("p", 0);
	webYaw = argFloat("y", 0);
	webThrottle = constrain(argFloat("th", 0), 0.0f, 1.0f);
	webSeq++;
	sendStatus();
}

static void handleConsole() {
	if (webServer.hasArg("cmd")) {
		xSemaphoreTake(webLock, portMAX_DELAY);
		webCommand = webServer.arg("cmd");
		xSemaphoreGive(webLock);
	}
	// return log since the given position
	uint32_t since = webServer.hasArg("since") ? webServer.arg("since").toInt() : 0;
	xSemaphoreTake(webLock, portMAX_DELAY);
	uint32_t head = webLogHead;
	if (head - since > WEB_LOG_SIZE - 1 || since > head) since = head > WEB_LOG_SIZE - 1 ? head - (WEB_LOG_SIZE - 1) : 0;
	String out;
	out.reserve(head - since + 16);
	out += String(head) + "\n";
	for (uint32_t i = since; i < head; i++) out += webLog[i % WEB_LOG_SIZE];
	xSemaphoreGive(webLock);
	webServer.send(200, "text/plain; charset=utf-8", out);
}

static void webTask(void *) {
	while (true) {
		webServer.handleClient();
		vTaskDelay(2);
	}
}

void setupWebRC() {
	if (wifiMode != W_AP && wifiMode != W_STA) return; // no IP network (disabled or ESP-NOW)
	print("Setup Web RC\n");
	webLock = xSemaphoreCreateMutex();
	webServer.on("/", [] { webServer.send_P(200, "text/html", WEBRC_PAGE); });
	webServer.on("/rc", handleRC);
	webServer.on("/status", sendStatus);
	webServer.on("/arm", [] { webRequest = 1; webServer.send(200, "text/plain", "ok"); });
	webServer.on("/disarm", [] { webRequest = 2; webServer.send(200, "text/plain", "ok"); });
	webServer.on("/mode", [] {
		String m = webServer.arg("m");
		webMode = m == "raw" ? RAW : m == "acro" ? ACRO : m == "stab" ? STAB : -1;
		webServer.send(200, "text/plain", "ok");
	});
	webServer.on("/console", handleConsole);
	webServer.onNotFound([] { webServer.send(404, "text/plain", "not found"); });
	webServer.begin();
	xTaskCreatePinnedToCore(webTask, "webrc", 6144, NULL, 1, NULL, 0);
	if (wifiMode == W_AP) print("Web RC: http://%s/\n", WiFi.softAPIP().toString().c_str());
	else print("Web RC: http://<drone IP>/ (see 'wifi' command)\n");
}

// Called from the main loop
void loopWebRC() {
	static uint32_t seq = 0;
	if (!webLock) return;

	if (webSeq != seq) {
		seq = webSeq;
		controlRoll = webRoll;
		controlPitch = webPitch;
		controlYaw = webYaw;
		controlThrottle = webThrottle;
		controlMode = NAN; // mode is set by the page buttons
		controlTime = t;
	}

	if (webRequest == 1) {
		if (controlThrottle < 0.05) armed = true;
		else print("Web RC: lower the throttle to arm\n");
		webRequest = 0;
	} else if (webRequest == 2) {
		armed = false;
		webRequest = 0;
	}

	if (webMode >= 0) {
		mode = webMode;
		webMode = -1;
	}

	if (webCommand.length()) {
		xSemaphoreTake(webLock, portMAX_DELAY);
		String cmd = webCommand;
		webCommand = "";
		xSemaphoreGive(webLock);
		print("> %s\n", cmd.c_str());
		doCommand(cmd, false);
	}
}
