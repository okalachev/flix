// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// sendWiFi and receiveWiFi implementations for the simulation

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/poll.h>
#include <gazebo/gazebo.hh>

int wifiMode = 1; // mock
int udpLocalPort = 14580;
int udpRemotePort = 14550;
const char *udpRemoteIP = "255.255.255.255";

int wifiSocket;

void setupWiFi() {
	wifiSocket = socket(AF_INET, SOCK_DGRAM, 0);
	sockaddr_in addr; // local address
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(udpLocalPort);
	if (bind(wifiSocket, (sockaddr *)&addr, sizeof(addr))) {
		gzerr << "Failed to bind WiFi UDP socket on port " << udpLocalPort << std::endl;
		return;
	}
	int broadcast = 1;
	setsockopt(wifiSocket, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)); // enable broadcast
	gzmsg << "WiFi UDP socket initialized on port " << udpLocalPort << " (remote port " << udpRemotePort << ")" << std::endl;
}

void sendWiFi(const uint8_t *buf, int len) {
	if (wifiSocket == 0) setupWiFi();
	sockaddr_in addr; // remote address
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(udpRemoteIP);
	addr.sin_port = htons(udpRemotePort);
	sendto(wifiSocket, buf, len, 0, (sockaddr *)&addr, sizeof(addr));
}

int receiveWiFi(uint8_t *buf, int len) {
	struct pollfd pfd = { .fd = wifiSocket, .events = POLLIN };
	if (poll(&pfd, 1, 0) <= 0) return 0; // check if there is data to read
	return recv(wifiSocket, buf, len, 0);
}
