// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix
// 耀坤整理版修改：https://github.com/YaoKunS/flix

// 无线通信：Wi-Fi（AP / STA）与 ESP-NOW
//
// ============================================================================
// 【本节是全工程最关键的改动：ESP-NOW 接收为什么要动原生 API】
//
// 现象：换成上游 2026-09 之后，耀坤自研的 ESP-NOW 遥控器连上飞机却"毫无反应"，
//   遥控器端显示已连接、飞机端串口也不报错，就是不动。2026-08 版是可以控的。
//
// 根因（读上游 ESP32_NOW_Serial 库源码确认）：
//   上游接收走 `espnow.read()`，而 ESPNOWSerial 是"**会话式**"的 —— 它只把
//   **已登记 peer** 发来的数据交上来。而登记 peer 的唯一途径是收到 4 字节
//   发现包 "flix"（ESP_NOW.onNewPeer 回调）。更要命的是，上游的发现包广播
//   还被 `if (espnow.isEncrypted())` 门控 —— 不加密就根本不广播。
//   于是：遥控器不知道飞机存在 → 不建会话 → 飞机永远收不到摇杆数据。
//
// 修法（沿用 2026-08 已验证的做法）：
//   1) 额外注册**原生** `esp_now_register_recv_cb()`，把收到的每个字节塞进
//      FreeRTOS 队列 recvQueue（1024 字节）。注意是"额外"——ESPNOWSerial 的
//      发送链路仍然保留，日志/发现包还能正常走。
//   2) `receiveWiFi()` 在 ESP-NOW 模式下优先从队列取字节，队列空才返回 0。
//   3) 发现包改为**无条件广播**（不再依赖是否加密），这样标准 flix 遥控代理
//      也能找到本机。
//   4) 默认信道 6：与耀坤英文版遥控器 `Control.hpp` 里的
//      `esp_wifi_set_channel(6)` 对齐。信道不一致时 ESP-NOW 物理层就收不到，
//      表现同样是"连着但不动"，且没有任何日志 —— 这是最难查的一类问题。
//
// 为什么用"字节队列"而不是"MavLink 帧队列"：
//   MAVLink 解析器本身是流式的（mavlink_parse_char 逐字节喂），队列里存字节
//   就等于把"网络接收"和"协议解析"解耦，解析不完整时不会丢帧。
// ============================================================================

#include <WiFi.h>
#include <ESP32_NOW_Serial.h>
#include <MacAddress.h>
#include <Preferences.h>
#include <esp_now.h>         // 原生 ESP-NOW API（会话式封装之外的那一层）
#include <freertos/queue.h>  // 接收队列
#include "util.h"
#include "i18n.h"

extern Preferences storage; // 使用主参数存储
extern float mavlinkTime;   // 安全层判失联用（sendWiFi 里判"该不该转广播"）

// 三种无线模式：关闭 / 热点 / 客户端 / ESP-NOW
const int W_DISABLED = 0, W_AP = 1, W_STA = 2, W_ESPNOW = 3;
int wifiMode = W_ESPNOW;

int wifiLongRange = 0;
int wifiBroadcast = 0; // 0 - 连上前先广播，1 - 一直广播
int udpLocalPort = 14550;
int udpRemotePort = 14550;
IPAddress udpRemoteIP = "255.255.255.255";
WiFiUDP udp;

// 发送仍用 ESPNOWSerial（这条链路已验证可用）
ESPNOWSerial espnow(NULL, 0, WIFI_IF_AP);
ESPNOWSerial espnowBroadcast(ESP_NOW.BROADCAST_ADDR, 0, WIFI_IF_AP);

// 【耀坤设置】信道 1：必须与遥控器一致
// 【为什么是 1 而不是 6，这是一次实测纠错（2026-10-18）】
//   早期按"耀坤英文版遥控器 Control.hpp 里 esp_wifi_set_channel(6)"把默认值定成 6，
//   但中文版遥控（ESP32-DJI-Dev-Yaokun20260815）**完全没调 esp_wifi_set_channel**，
//   走的是 Wi-Fi 默认信道 1。于是飞机听 6、遥控发 1，物理层彻底错开 ——
//   现象极具迷惑性：遥控器"对端发现"页能列出飞机（扫描是跨信道的），
//   点选后也显示"已连接"，但飞机 rxPackets 恒为 0，rc 全 0。
//   现以已确认可用的 D:\YaoKunS\Flix-YaoKun\flix 为准，取 1。
// ⚠️ 换遥控器（英文版 / 其他固件）时必须两边对齐：控制台 `ch <1-13>` 改飞机端。
int espnowChannel = 1;

// ---- 原生接收队列与回调 ----
static QueueHandle_t recvQueue = nullptr;
static volatile uint32_t lostPackets = 0;
static bool espnowRecvInitialized = false;
// 【诊断计数器】接遥控排查用：
//   rxPackets = 收到的 ESP-NOW 包数（含任何来源，不限于遥控器）
//   rxBytes   = 收到的总字节数（与 rxPackets 一起判断是"完全没包"还是"收到但解析不了"）
//   rxLastMac = 最近一个发包方的 MAC，能直接看出是不是遥控器在发
// 三个都不写 NVS，纯运行时统计，reboot 清零。
static volatile uint32_t rxPackets = 0;
static volatile uint32_t rxBytes = 0;
static volatile uint8_t rxLastMac[6] = {0, 0, 0, 0, 0, 0};

// 已登记对端 MAC 表（用于"自动学习遥控器"）
// 【为什么需要它】ESP-NOW 的硬性规则：**只有登记在 peer 表里的 MAC 发来的包才会
// 触发接收回调**，未登记的来源在驱动层就被丢弃，回调根本不会进来。
// 耀坤遥控器（2026-04 起的版本）在「对端发现」页点选飞机后会 `connect(mac)`，
// 之后所有摇杆数据都**单播**给飞机 MAC —— 而飞机端原来只注册了接收回调、
// 从不 add_peer，于是永远收不到（2026-10-18 台架实测：rxPackets 恒为 0，
// 而遥控器侧显示已连接，因为反向的飞机广播它能收到，形成单向通）。
// 解法：① 预登记广播地址（发现阶段的包能进来）；② 收到未知来源时自动登记。
static const int MAX_LEARNED_PEERS = 4;
static uint8_t learnedPeers[MAX_LEARNED_PEERS][6];
static volatile int learnedPeerCount = 0;

static bool addPeerMac(const uint8_t *mac); // 前向声明（initNativeRecv 里要用）

// 判断某 MAC 是否已登记
static bool isPeerKnown(const uint8_t *mac) {
	static const uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	bool isBcast = true;
	for (int i = 0; i < 6; i++) if (mac[i] != bcast[i]) { isBcast = false; break; }
	if (isBcast) return true;
	if (esp_now_is_peer_exist(mac)) return true;
	for (int k = 0; k < learnedPeerCount; k++) {
		bool same = true;
		for (int i = 0; i < 6; i++) if (learnedPeers[k][i] != mac[i]) { same = false; break; }
		if (same) return true;
	}
	return false;
}

// 把一个 MAC 加入 ESP-NOW peer 表（不加密、走 AP 接口）
// 【必须在主循环上下文调用】esp_now_add_peer 内部会取 Wi-Fi 锁，
// 在中断/回调里调用有死锁风险，所以回调里只置标志、由 learnPeerIfNeeded() 落地。
static bool addPeerMac(const uint8_t *mac) {
	esp_now_peer_info_t peer = {};
	memcpy(peer.peer_addr, mac, 6);
	peer.channel = 0;              // 0 = 使用当前信道
	peer.ifidx = WIFI_IF_AP;       // 与 ESPNOWSerial 一致
	peer.encrypt = false;
	return esp_now_add_peer(&peer) == ESP_OK;
}

// 待学习的 MAC（回调里写，主循环读）
static volatile bool peerLearnPending = false;
static volatile uint8_t peerLearnMac[6] = {0, 0, 0, 0, 0, 0};

// 在主循环里落地"自动学习"（由 processMavlink / handleWiFi 周期调用）
void learnPeerIfNeeded() {
	if (!peerLearnPending) return;
	peerLearnPending = false;
	uint8_t mac[6];
	for (int i = 0; i < 6; i++) mac[i] = peerLearnMac[i];
	if (isPeerKnown(mac)) return;
	if (learnedPeerCount >= MAX_LEARNED_PEERS) return;
	if (addPeerMac(mac)) {
		memcpy(learnedPeers[learnedPeerCount], mac, 6);
		learnedPeerCount++;
		print(TXT_WIFI_PEER_LEARNED,
			mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	}
}

// ESP-NOW 接收回调（运行在 Wi-Fi 任务上下文，必须放在 IRAM）
// 【为什么逐字节入队】队列元素是 1 字节，xQueueSendFromISR 一次一个。
//   队列满时累加 lostPackets 并**丢弃**——宁可丢旧数据也不要阻塞中断，
//   否则 Wi-Fi 任务被拖住会导致整机卡顿。
static void IRAM_ATTR onReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
	if (!espnowRecvInitialized) return;
	// 统计：即使队列满也要记，这样"收到过"这个事实不会丢
	rxPackets++;
	rxBytes += len;
	if (info != NULL && info->src_addr != NULL) {
		for (int i = 0; i < 6; i++) rxLastMac[i] = info->src_addr[i];
		// 记下来源 MAC，交给主循环决定是否登记（回调里不能调 esp_now_add_peer）
		for (int i = 0; i < 6; i++) peerLearnMac[i] = info->src_addr[i];
		peerLearnPending = true;
	}
	for (int i = 0; i < len; i++) {
		if (xQueueSendFromISR(recvQueue, &data[i], NULL) != pdTRUE) {
			lostPackets++;
		}
	}
}

// 注册原生接收回调。
// 注意：**不重新初始化 ESP-NOW** —— ESPNOWSerial 已经调用过 esp_now_init()，
// 再调一次会返回 ESP_ERR_ESPNOW_INTERNAL 并把已有 peer 全清掉。
//
// 【顺序坑，勿调换】必须先创建队列、再注册回调。
// 若先注册回调，从注册成功那一刻起回调就可能被触发，而那时 recvQueue 还是 nullptr，
// xQueueSendFromISR(nullptr, ...) 在中断上下文里是未定义行为 —— 轻则丢包，重则崩溃。
// 2026-08 验证版就是"先注册后建队列"，本版曾调换顺序，属实现细节回归。
static bool initNativeRecv() {
	recvQueue = xQueueCreate(1024, sizeof(uint8_t));
	if (recvQueue == NULL) {
		print(TXT_WIFI_QFAIL);
		return false;
	}
	// 【预登记广播 peer】遥控器在"对端发现"阶段的探索包是广播发的；
	// 未登记广播地址的话连探索包都进不来，自动学习就永远没有触发机会。
	// 这里失败不致命（ESPNOWSerial 可能已经登记过），打印出来便于排查。
	{
		static const uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
		if (!esp_now_is_peer_exist(bcast)) addPeerMac(bcast);
	}
	if (esp_now_register_recv_cb(onReceive) != ESP_OK) {
		print(TXT_WIFI_CBFAIL);
		return false;
	}
	espnowRecvInitialized = true;
	return true;
}

void setupWiFi() {
	print(TXT_SETUP_WIFI);
	WiFi.enableLongRange(wifiLongRange);

	// ------------------------------------------------------------------------
	// 【热点名统一前缀 YaoKun_】本函数**两条路径**都要设，别只改 W_AP 那条。
	//
	// 为什么：ESP-NOW 模式同样要把射频切到 AP 模式（ESP-NOW 只能在 AP/STA 的
	//   Wi-Fi 接口上跑），`WiFi.mode(WIFI_AP)` 一调用，arduino-esp32 的 core 就会
	//   **自动起一个默认热点**，SSID 是 `ESP_` + MAC 后三字节、密码空。
	//   于是"飞机在 ESP-NOW 模式下也在广播热点，但名字是 ESP_xxxxxx"，
	//   现场既认不出是耀坤的机器，也可能被误连。所以两条路径统一走下面这段命名。
	//
	// 取值优先级：
	//   ① NVS 里用户用 `ap <热点名> <密码>` 设过的名字（WIFI_AP_SSID）；
	//   ② 都没有时 = 品牌前缀 "YaoKun_" + 芯片 MAC 末两字节，例：YaoKun_F224。
	//
	// 为什么取 MAC 末两字节而不是数字 1/2：
	//   1/2 得靠人工烧不同固件，违反"同一份固件通吃所有板子"的原则；
	//   MAC 出厂唯一，任何一块板子自动就不同，且打印出来方便肉眼对应。
	//
	// ⚠️ 【实测提醒】这里取的是 **AP 模式 MAC**，不等于 `wifi` 命令打印的"本机 MAC"。
	//   1 号机实测：`wifi` 显示 STA MAC = 80:65:99:DF:F2:24（ESP-NOW 用的是它），
	//   而 `WiFi.macAddress()` 在 AP 模式下返回的是 AP MAC，末两字节为 62:02
	//   → 热点名 = **YaoKun_6202**，不是直觉上的 "YaoKun_F224"。
	//   原因是 ESP32 的 AP MAC 与 STA MAC 差在第一个字节的第 1 位（AP 置 1、STA 清 0），
	//   末两字节只在该位跨越进位时才变，表现就是"看起来像取反"。
	//   结论：**别拿 wifi 命令的 MAC 去猜热点名**，开机日志里那行"当前热点名"才是准的。
	// ------------------------------------------------------------------------
	String apSsid = storage.getString("WIFI_AP_SSID", "");
	if (apSsid.isEmpty()) {
		uint8_t mac[6];
		WiFi.macAddress(mac); // 用芯片出厂 MAC（STA 基址），不依赖 AP 模式是否已启动
		char suffix[5];
		snprintf(suffix, sizeof(suffix), "%02X%02X", mac[4], mac[5]);
		apSsid = String("YaoKun_") + suffix;
	}
	const char *apPass = storage.getString("WIFI_AP_PASS", "12345678").c_str();

	if (wifiMode == W_AP) {
		WiFi.softAP(apSsid.c_str(), apPass);
		print(TXT_WIFI_AP_DEFAULT, apSsid.c_str()); // 把实际生效的热点名打出来，便于现场辨认
		udp.begin(udpLocalPort);
	}

	if (wifiMode == W_STA) {
		WiFi.begin(storage.getString("WIFI_STA_SSID", "").c_str(),
			storage.getString("WIFI_STA_PASS", "").c_str());
		udp.begin(udpLocalPort);
	}

	if (wifiMode == W_ESPNOW) {
		// 先切模式（core 会顺手起一个 ESP_ 默认热点），紧接着用软 AP 覆盖成 YaoKun_ 名字。
		// 顺序不能颠倒：softAP() 必须在 mode(WIFI_AP) 之后，否则会被模式切换重置掉。
		WiFi.mode(WIFI_AP);
		WiFi.softAP(apSsid.c_str(), apPass);
		WiFi.setChannel(espnowChannel);
		print(TXT_WIFI_CHANNEL, espnowChannel);
		print(TXT_WIFI_AP_DEFAULT, apSsid.c_str());

		// 初始化 ESPNOWSerial（发送用途）
		espnow.addr(MacAddress(storage.getString("ESPNOW_PEER_MAC", "FF:FF:FF:FF:FF:FF").c_str()));
		String key = storage.getString("ESPNOW_PEER_KEY", "");
		espnow.setKey(key.isEmpty() ? nullptr : (const uint8_t *)key.c_str());
		espnow.begin();
		espnowBroadcast.begin();

		// 额外注册原生接收回调（接收遥测/摇杆数据）
		if (initNativeRecv()) {
			print(TXT_WIFI_NATIVE_OK);
		} else {
			print(TXT_WIFI_NATIVE_FAIL);
		}
	}

	WiFi.setSleep(false); // 关闭省电，否则 ESP-NOW 延迟抖动极大
}

void sendWiFi(const uint8_t *buf, int len) {
	if (espnow) {
		espnow.write(buf, len);

		// 定期广播发现包，帮助遥控器/代理找到本机。
		// 【与上游不同】上游是 `if (espnow.isEncrypted() && discovery)`，不加密就不广播，
		// 导致未加密的第三方遥控器永远发现不了飞机。这里去掉加密条件。
		static Rate discovery(2);
		if (discovery) {
			espnowBroadcast.write((const uint8_t *)"flix", 4);
		}
		return;
	}

	if (WiFi.softAPgetStationNum() == 0 && !WiFi.isConnected()) return;

	bool broadcast = wifiBroadcast || !(t - mavlinkTime < 5); // 失联时改广播
	udp.beginPacket(broadcast ? IPAddress(255, 255, 255, 255) : udpRemoteIP, udpRemotePort);
	udp.write(buf, len);
	udp.endPacket();
}

int receiveWiFi(uint8_t *buf, int len) {
	if (wifiMode == W_ESPNOW) {
		// 优先走原生队列（这是遥控能不能控的关键路径）
		if (!espnowRecvInitialized || recvQueue == NULL) return 0;
		int i = 0;
		for (; i < len; i++) {
			if (xQueueReceive(recvQueue, &buf[i], 0) != pdTRUE) break;
		}
		return i;
	}

	// UDP 模式
	if (WiFi.softAPgetStationNum() == 0 && !WiFi.isConnected()) return 0;
	udp.parsePacket();
	if (udp.remoteIP()) udpRemoteIP = udp.remoteIP();
	return udp.read(buf, len);
}

void printWiFiInfo() {
	if (espnow) {
		print(TXT_WIFI_MODE_ESPNOW);
		print(TXT_WIFI_VERSION, ESP_NOW.getVersion());
		print(TXT_WIFI_MAXLEN, ESP_NOW.getMaxDataLen());
		print(TXT_WIFI_MAC, WiFi.softAPmacAddress().c_str());
		print(TXT_WIFI_PEER, MacAddress(espnow.addr()).toString().c_str());
		print(TXT_WIFI_ENCRYPTED, espnow.isEncrypted());
		print(TXT_WIFI_CHANNEL, espnowChannel);
		print(TXT_WIFI_LOST, espnow.lost);
		print(TXT_WIFI_NATIVE, espnowRecvInitialized);
		if (recvQueue != NULL) {
			print(TXT_WIFI_QBYTES, uxQueueMessagesWaiting(recvQueue));
		}
		// 额外给出"接收丢包"，与发送丢包（espnow.lost）区分开 —— 排查时一看就知道
		// 是"发不出去"还是"收不过来"
		print("接收丢弃字节数：%u\n", lostPackets);
		// 【遥控排查三件套】收包数 / 收字节数 / 最近来源 MAC
		// 判读方法：
		//   包数=0            → 物理层根本没收到，查信道、遥控器是否真的在发
		//   包数>0 但 rc 全 0 → 收到的是别人的包或非 MANUAL_CONTROL，打印 MAC 定位来源
		//   包数在涨、rc 正常 → 链路健康
		print(TXT_WIFI_RXPACKETS, rxPackets);
		print(TXT_WIFI_RXBYTES, rxBytes);
		print(TXT_WIFI_RXMAC,
			rxLastMac[0], rxLastMac[1], rxLastMac[2], rxLastMac[3], rxLastMac[4], rxLastMac[5]);
		// 【MAVLink 解析诊断】遥控排查关键三行：
		//   帧数=0        → 收到字节但组不成 MAVLink 帧（协议不匹配 / Raw 格式）
		//   帧数>0 但 id≠69 → 遥控器发的是 HEARTBEAT 之类，没发 MANUAL_CONTROL
		//   id=69 且 rc 仍 0 → 被 target 过滤挡了，看来源 sysid
		extern uint32_t rxMsgCount, rxLastMsgId;
		extern uint8_t rxLastSrcSys;
		print(TXT_WIFI_MSGS, rxMsgCount);
		print(TXT_WIFI_LASTMSG, rxLastMsgId);
		print(TXT_WIFI_LASTSYS, rxLastSrcSys);
	} else if (WiFi.getMode() == WIFI_MODE_AP) {
		print(TXT_WIFI_AP);
		print(TXT_WIFI_MAC, WiFi.softAPmacAddress().c_str());
		print(TXT_WIFI_SSID, WiFi.softAPSSID().c_str());
		print(TXT_WIFI_PASS_MASK);
		print(TXT_WIFI_CHANNEL, WiFi.channel());
		print(TXT_WIFI_CLIENTS, WiFi.softAPgetStationNum());
		print(TXT_WIFI_IP, WiFi.softAPIP().toString().c_str());
		print(TXT_WIFI_REMOTE_IP, udpRemoteIP.toString().c_str());
	} else if (WiFi.getMode() == WIFI_MODE_STA) {
		print(TXT_WIFI_STA);
		print(TXT_WIFI_CONNECTED, WiFi.isConnected());
		print(TXT_WIFI_MAC, WiFi.macAddress().c_str());
		print(TXT_WIFI_SSID, WiFi.SSID().c_str());
		print(TXT_WIFI_PASS_MASK);
		print(TXT_WIFI_CHANNEL, WiFi.channel());
		print(TXT_WIFI_RSSI, WiFi.RSSI());
		print(TXT_WIFI_IP, WiFi.localIP().toString().c_str());
		print(TXT_WIFI_REMOTE_IP, udpRemoteIP.toString().c_str());
	} else {
		print(TXT_WIFI_OFF);
	}
	print(TXT_WIFI_MAVLINK, valid(mavlinkTime));
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
		print(TXT_WIFI_INVALID);
		return;
	}
	print(TXT_WIFI_REBOOT);
}

void setWiFiMode(const String& mode) {
	if (mode == "ap") {
		wifiMode = W_AP;
	} else if (mode == "sta") {
		wifiMode = W_STA;
	} else if (mode == "espnow") {
		wifiMode = W_ESPNOW;
	} else if (mode == "off") {
		wifiMode = W_DISABLED;
	} else {
		print(TXT_WIFI_MODE_INVALID);
		return;
	}
	static const char *modes[] = {"关闭", "热点(AP)", "客户端(STA)", "ESP-NOW"};
	print(TXT_WIFI_MODE_SET, modes[wifiMode]);
}
