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
#include <esp_mac.h>         // esp_read_mac：直接从 eFuse 读各接口 MAC，不依赖当前 Wi-Fi 模式
#include <freertos/queue.h>  // 接收队列
#include "util.h"
#include "i18n.h"

extern Preferences storage; // 使用主参数存储
extern float mavlinkTime;   // 安全层判失联用（sendWiFi 里判"该不该转广播"）

// ============================================================================
// 【无线模式取值】五种：关闭 / 热点 / 客户端 / ESP-NOW（纯）/ ESP-NOW + 热点（共存）
//
// 为什么要加 W_BOTH（2026-11-05 用户需求）：
//   原版 `wifi ap|sta|espnow` 是**三选一**，ESP-NOW 模式下虽然也会起热点
//   （ESP-NOW 必须挂在 AP/STA 接口上，`WiFi.mode(WIFI_AP)` 一起 core 就起热点），
//   但 `udp.begin()` 只在 W_AP/W_STA 分支里调用 ⇒ 地面站连上热点也没有 MAVLink 应答；
//   同时 `sendWiFi()` 里 `if (espnow) { ...; return; }` 直接返回 ⇒ 遥测永远不走 UDP。
//   物理层本来就能共存，缺的只是"UDP 也开、发完 ESP-NOW 别急着 return"。
//
// 为什么默认值取 W_BOTH 而不是 W_ESPNOW：
//   目标用户是"照着开源文档复刻"的新人，装好后应当**遥控能控、手机连上就能看遥测**，
//   不该再要求他先跑一条串口命令。所以出厂默认 = 共存。
//
// ⚠️【NVS 覆盖坑，别只改这一行就以为生效】
//   `WIFI_MODE` 是持久化参数（parameters.ino:162），`setupParameters()` 会用
//   NVS 里的旧值覆盖这里的编译默认值。老用户板子里存的是 3（纯 ESP-NOW），
//   只改默认值 → 开机仍读回 3，表现为"改了没反应"。
//   故在 setupWiFi() 开头调用 migrateWifiModeToCoexist() 做一次性迁移（见下）。
// ============================================================================
const int W_DISABLED = 0, W_AP = 1, W_STA = 2, W_ESPNOW = 3, W_BOTH = 4;
int wifiMode = W_BOTH;

int wifiLongRange = 0;
int wifiBroadcast = 0; // 0 - 连上前先广播，1 - 一直广播
int udpLocalPort = 14550;
int udpRemotePort = 14550;
IPAddress udpRemoteIP = "255.255.255.255";
WiFiUDP udp;

// UDP 通道是否已启用（W_AP / W_STA / W_BOTH 会置位；纯 ESP-NOW 保持 false）。
// 【为什么单独立这个标志，而不是到处判 wifiMode】
//   sendWiFi / receiveWiFi 里要表达的是"能不能走 UDP"这一件事，
//   而 wifiMode 混合了"射频接口怎么配"和"链路开哪几条"两种语义。
//   用一个只会被 setupWiFi 置位的布尔量，收发路径就不必跟着模式增加分支，
//   将来再加模式（比如 STA+AP 共存）也只改 setupWiFi 一处。
bool udpStarted = false;

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

// ----------------------------------------------------------------------------
// 共存模式"一次性迁移"（老板子升级到本版固件时用）
//
// 【为什么必须有这一步】
//   `wifiMode` 是持久化参数（见 parameters.ino 的 {"WIFI_MODE", &wifiMode}），
//   `setupParameters()` 会拿 NVS 里的旧值覆盖 wifi.ino 顶部写的编译默认值。
//   升级前用户板子里存的是 3（纯 ESP-NOW），**只把默认值改成 W_BOTH(4) 是无效的**，
//   开机照旧读回 3 —— 表现为"我改了代码、烧了固件，怎么还是老样子"。
//   （这也是本工程历史上反复踩过的"改了没反应"类问题的同一个根。）
//
// 【为什么用版本号而不是每次开机强制改】
//   用户可能确实想长期用纯 ESP-NOW（`wifi espnow`）来二分排查空口干扰。
//   若无条件改回共存，他的显式选择每次重启都会被抹掉。
//   所以只迁移一次：写 NVS 标记键 COEX_VER，之后永不再动这个参数。
//
// 【只改 3，不改别的】W_AP / W_STA / W_DISABLED 都是用户显式选过的模式，
//   迁移必须尊重它们，否则会把"只连路由器"的用户强行改成热点+ESP-NOW。
//
// 【键名长度】NVS 键上限 15 字符，"COEX_VER" 用 8 个，安全。
// ----------------------------------------------------------------------------
static void migrateWifiModeToCoexist() {
	const uint32_t COEX_CONF_VER = 2; // 2 = 本版：默认共存 + 热点名改读 eFuse MAC
	// 越界保护：NVS 是用户可写的（`p WIFI_MODE 9` 也能存进去），非法值会让
	// setupWiFi() 一个分支都进不去 → 射频完全不启动，现象是"开机没热点、遥控也不通"。
	// 放在迁移之前做，顺带把异常板子救回来。
	if (wifiMode < W_DISABLED || wifiMode > W_BOTH) {
		wifiMode = W_BOTH;
	}
	if (storage.getUInt("COEX_VER", 0) >= COEX_CONF_VER) return;
	if (wifiMode == W_ESPNOW) { // 3 = 老固件的默认值（纯 ESP-NOW）
		wifiMode = W_BOTH;
		print(TXT_WIFI_COEX_MIGRATED);
	}
	storage.putUInt("COEX_VER", COEX_CONF_VER);
}

// ----------------------------------------------------------------------------
// 热点出厂密码
//
// 【为什么写成字节表，而不是写一串 8 个数字的字符串字面量】
//   1) 合规/铁律：耀坤产品线的出厂热点密码是"逐位一、二、三、四、五、六、七、八"，
//      约定要求任何地方都**不要连写这 8 位数字**（历史上连写形态在文档/终端
//      传输链路里被改动过多次）。用 ASCII 码表逐字节写出，源码里不存在该数字串，
//      任何人都能按码表核对，且不会被文本链路篡改。
//   2) 因此这里的字节值必须逐位对应：
//        49='1' 50='2' 51='3' 52='4' 53='5' 54='6' 55='7' 56='8'
//      ⚠️ 改这个表 = 改出厂密码，属于"未经用户明确允许禁止修改"的范围。
//   3) 末尾的 0 是 C 字符串结束符，必须保留（本数组按 const char* 使用）。
// ----------------------------------------------------------------------------
static const char DEFAULT_AP_PASS[] = {49, 50, 51, 52, 53, 54, 55, 56, 0};

// ----------------------------------------------------------------------------
// 判断一个 Wi-Fi 密码是否可用（热点与 STA 共用）
//
// 参数：p —— 待检查的密码字符串（可为 nullptr）
// 返回：true = 可交给 softAP()/WiFi.begin() 使用；false = 必须丢弃
//
// 【为什么需要这个函数（2026-11-05 实测踩坑，现象+根因+修法）】
//   现象：用户反馈"手机连不上飞机热点，密码输一、二、三…七八 一直是错的"。
//   根因：`ap <热点名> <密码>` 命令当年只做了 `strlen(second) >= 8` 这一条校验，
//     而用户照文档敲的是**占位符本身**：`ap <热点名> <密码>`。
//     `<密码>` 的 UTF-8 是 60,229,175,134,231,160,129,62 —— **恰好 8 字节**，
//     刚好通过 >=8 检查，被原样写进 NVS 的 WIFI_AP_PASS，又被驱动写进
//     net80211 的 ap.passwd。于是热点真实密码成了 `<密码>` 这 4 个汉字形状的字节，
//     用户按文档输"一二三四五六七八"当然连不上，而且看代码完全看不出问题
//     （代码里的默认值一直是对的，是 NVS 覆盖了它）。
//     NVS 原始字节实证：0x024E8 处 ap.passwd = [60,229,175,134,231,160,129,62]。
//   修法：
//     ① 读取端（setupWiFi）—— 判为不可用就 remove 掉 NVS 项，回退出厂默认；
//     ② 写入端（configWiFi）—— 直接拒绝，别让脏值再进 NVS；
//     ③ 校验规则覆盖三类脏值：长度越界、占位符尖括号、非 ASCII（中文等）。
//
// 【各条规则的依据】
//   - 8~63：802.11 对 WPA2-PSK 口令（passphrase）的硬性长度约束，
//     少于 8 位 core 会拒（退化成开放网络），多于 63 位被截断，都会让用户"输对了也连不上"。
//   - 可打印 ASCII（0x20~0x7E）：手机/Windows 输中文密码时输入法、编码、全角半角
//     任一处不同就失败，现场无法排查；出厂密码是数字，没有任何理由留中文。
//   - 不含 '<' '>'：文档占位符特征。真用户不会用尖括号做密码，
//     但照抄文档的人一定会 —— 这正是本次事故的触发方式。
// ----------------------------------------------------------------------------
static bool isUsableWifiPass(const char *p) {
	if (p == nullptr) return false;
	size_t n = strlen(p);
	if (n < 8 || n > 63) return false; // 802.11 WPA2-PSK 长度约束
	for (size_t i = 0; i < n; i++) {
		const unsigned char c = (unsigned char)p[i];
		if (c < 0x20 || c > 0x7E) return false;  // 非可打印 ASCII（中文/控制字符）
		if (c == '<' || c == '>') return false;  // 文档占位符特征
	}
	return true;
}

// 同上，但用于热点名：只挡占位符与超长，不限制字符集（个别用户确实要用中文热点名）
static bool isUsableApName(const char *p) {
	if (p == nullptr || p[0] == '\0') return false;
	size_t n = strlen(p);
	if (n > 32) return false; // 802.11 SSID 硬上限
	for (size_t i = 0; i < n; i++) {
		if (p[i] == '<' || p[i] == '>') return false; // 占位符特征
	}
	return true;
}

void setupWiFi() {
	print(TXT_SETUP_WIFI);
	migrateWifiModeToCoexist(); // 必须在任何 wifiMode 判断之前执行（见上）
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
	// ⚠️ 【2026-11-05 修正：MAC 必须从 eFuse 读，不能问 WiFi 对象要】
	//   旧写法 `WiFi.macAddress(mac)` 在 AP/ESP-NOW 上下文中返回的是 **AP 接口 MAC**，
	//   与 `wifi` 命令打印的"本机 MAC"（STA MAC）不一致：
	//     1 号机：STA MAC = 80:65:99:DF:F2:24（`wifi` 显示、ESP-NOW 实际使用）
	//             AP  MAC 末两字节 = 62:02 → 旧热点名 YaoKun_6202
	//   用户诉求是"热点名 = YaoKun_ + MAC 后 4 位，每台飞机都不一样"，"后 4 位"指的
	//   就是 `wifi` 命令里那串 MAC 的末两字节（1 号机 = F224）。
	//   所以改用 IDF 底层 `esp_read_mac(ESP_MAC_WIFI_STA)`：直接读芯片 eFuse 出厂 MAC，
	//   与当前 Wi-Fi 模式无关，永远返回一致的值 → 热点名与 `wifi` 输出能对得上。
	//
	//   ⚠️ 代码里**没有任何硬编码后缀**：后缀完全由芯片熔丝位生成，
	//   1 号机 = YaoKun_F224、2 号机 = YaoKun_DE38……每块板子天然不同，
	//   同一份固件通吃所有板子（这正是"取 MAC 而不是 1/2 编号"的原因）。
	//   若用户用 `ap <名字> <密码>` 手动改过名（NVS 里有 WIFI_AP_SSID），
	//   下面第一行就会取到它，自动命名不生效 —— 手填优先，保持老行为。
	// ------------------------------------------------------------------------
	String apSsid = storage.getString("WIFI_AP_SSID", "");
	// 【合法性校验（2026-11-05 实测发现）】
	//   1 号机 NVS 里的 WIFI_AP_SSID 是字面 **"<热点名>"** —— 用户当年照文档
	//   把占位符当参数敲进了 `ap <热点名> <密码>`。后果：`apSsid.isEmpty()` 为假，
	//   自动命名被跳过，手机搜到的热点就叫 `<热点名>`，与 YaoKun_ 品牌命名完全脱节，
	//   而且现场根本认不出是哪台飞机（本次就是靠这个现象排查出来的）。
	//   下面把明显非法的值丢弃并从 NVS 删除，回退到自动命名：
	//     - 空串（正常走自动命名）
	//     - 长度 > 32：802.11 SSID 硬上限，超长会被 core 截断
	//     - 含 '<' / '>'：占位符特征，几乎不可能是用户真想要的
	bool apSsidInvalid = !isUsableApName(apSsid.c_str());
	if (apSsidInvalid) {
		if (!apSsid.isEmpty()) {
			storage.remove("WIFI_AP_SSID"); // 清掉脏值，别让它每次开机都被读到
			print(TXT_WIFI_AP_NAME_DROPPED);
		}
		uint8_t mac[6];
		esp_read_mac(mac, ESP_MAC_WIFI_STA); // eFuse 出厂 STA MAC，与模式无关
		char suffix[5];
		snprintf(suffix, sizeof(suffix), "%02X%02X", mac[4], mac[5]);
		apSsid = String("YaoKun_") + suffix;
	}
	// 【注意 apPass 的生命周期】原写法 `const char *apPass = storage.getString(...).c_str();`
	//   拿的是**临时 String 对象**内部缓冲的指针，语句结束后临时对象析构 ⇒ 悬垂指针。
	//   以前只在一个分支里用一次，没暴露出问题；共存模式要多次使用，故改成具名 String，
	//   保证 c_str() 指向的缓冲在整个 setupWiFi 期间有效。
	String apPassStr = storage.getString("WIFI_AP_PASS", DEFAULT_AP_PASS);
	// 【脏密码自动清洗（2026-11-05，本次事故的正面修复）】
	//   NVS 里可能残留历史误写入的值（典型：`<密码>` 占位符，UTF-8 恰好 8 字节，
	//   骗过了当年那条 `strlen >= 8` 的校验）。这类值会让 softAP 用一个谁都不知道的
	//   密码起热点，用户在手机上怎么输都不对，而代码里的默认值明明是对的 —— 极难排查。
	//   这里统一在**使用前**判定：不可用就删掉 NVS 项并回退出厂默认，同时打印一行提示，
	//   这样现场从开机日志就能看出"密码曾被清洗过"，不用再烧固件去猜。
	//   ⚠️ 注意顺序：必须在 `apPass = apPassStr.c_str()` 之前完成替换，
	//   否则 apPass 会指向被重新赋值前的旧缓冲（String 重新赋值会换堆块）。
	if (!isUsableWifiPass(apPassStr.c_str())) {
		if (!apPassStr.isEmpty()) {
			storage.remove("WIFI_AP_PASS"); // 清掉脏值，别让它每次开机都生效
			print(TXT_WIFI_AP_PASS_DROPPED);
		}
		apPassStr = DEFAULT_AP_PASS; // 回退到出厂密码（字节表，见文件上部）
	}
	const char *apPass = apPassStr.c_str();

	if (wifiMode == W_AP) {
		WiFi.softAP(apSsid.c_str(), apPass);
		print(TXT_WIFI_AP_DEFAULT, apSsid.c_str()); // 把实际生效的热点名打出来，便于现场辨认
		udp.begin(udpLocalPort);
		udpStarted = true;
	}

	if (wifiMode == W_STA) {
		WiFi.begin(storage.getString("WIFI_STA_SSID", "").c_str(),
			storage.getString("WIFI_STA_PASS", "").c_str());
		udp.begin(udpLocalPort);
		udpStarted = true;
	}

	// W_ESPNOW = 纯 ESP-NOW（不开 UDP，留给排查用）；W_BOTH = ESP-NOW + 热点共存
	if (wifiMode == W_ESPNOW || wifiMode == W_BOTH) {
		// 先切模式（core 会顺手起一个 ESP_ 默认热点），紧接着用软 AP 覆盖成 YaoKun_ 名字。
		// 顺序不能颠倒：softAP() 必须在 mode(WIFI_AP) 之后，否则会被模式切换重置掉。
		//
		// 【信道必须与 ESP-NOW 一致 —— 共存模式的第一硬约束】
		//   AP 与 ESP-NOW 共用同一射频，信道不一致 = 遥控物理层直接失联（没有任何报错）。
		//   这里把 espnowChannel 作为 softAP() 的第三个参数**一次设定到位**，
		//   不再沿用"先 softAP() 走默认信道、再 setChannel()"的两步写法：
		//   二次切信道会让已连入的客户端掉线，且切换瞬间信道短暂不一致，遥控可能丢包。
		WiFi.mode(WIFI_AP);
		WiFi.softAP(apSsid.c_str(), apPass, espnowChannel);
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

		// 【共存模式】再把 UDP 打开：QGC / 手机地面站连上本机热点后，
		//   MAVLink 走 192.168.4.1:14550 收发。两条链路各走各的：
		//   - 遥控摇杆：ESP-NOW（低延迟，主控制链路，优先）
		//   - 地面站：  UDP over AP（遥测、参数、指令）
		// 纯 ESP-NOW 模式**故意不碰 UDP**，保持与上游一致的"单链路"行为，
		// 便于出现干扰/丢包时二分定位到底是哪条链路的问题。
		if (wifiMode == W_BOTH) {
			udp.begin(udpLocalPort);
			udpStarted = true;
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

		// 【共存模式】ESP-NOW 发完**不再直接 return**，继续往下走 UDP 给地面站。
		//   - 纯 ESP-NOW 模式：udpStarted 恒为 false → 直接返回，行为与旧版完全一致；
		//   - 共存模式：只有"AP 上真的有客户端连入"时才发 UDP。没人连就跳过，
		//     把空口时间全留给遥控 —— ESP-NOW 与 AP 共用同一射频，这是省出来的。
		//   ⚠️ 判据用 udpStarted（而不是 wifiMode），是为了让这里不依赖模式枚举。
		if (!udpStarted || WiFi.softAPgetStationNum() == 0) return;
	}

	if (!udpStarted) return; // 理论到不了（无 ESP-NOW peer 且没开 UDP），留作兜底
	if (WiFi.softAPgetStationNum() == 0 && !WiFi.isConnected()) return;

	bool broadcast = wifiBroadcast || !(t - mavlinkTime < 5); // 失联时改广播
	udp.beginPacket(broadcast ? IPAddress(255, 255, 255, 255) : udpRemoteIP, udpRemotePort);
	udp.write(buf, len);
	udp.endPacket();
}

int receiveWiFi(uint8_t *buf, int len) {
	if (wifiMode == W_ESPNOW || wifiMode == W_BOTH) {
		// 优先走原生队列（这是遥控能不能控的关键路径）
		if (!espnowRecvInitialized || recvQueue == NULL) return 0;
		int i = 0;
		for (; i < len; i++) {
			if (xQueueReceive(recvQueue, &buf[i], 0) != pdTRUE) break;
		}
		// 【共存模式】ESP-NOW 队列这一轮取空了，才顺手取一帧 UDP（地面站下行指令）。
		//
		// 【为什么"队列非空就完全不碰 UDP"，而不是两路都读、拼进同一个缓冲】
		//   MAVLink 解析器是**流式**的（mavlink_parse_char 逐字节喂），它会把
		//   两路字节当成同一条流。ESP-NOW 与 UDP 的帧交错拼在一起，会让解析器
		//   在两条流之间反复 resync，轻则丢帧、重则读出错误的载荷长度。
		//   所以这里严格串行：要么喂 ESP-NOW 的字节，要么喂 UDP 的，绝不混。
		//
		// 代价（已知取舍）：遥控器高频发包时（约 50 Hz），若某几轮队列恰好非空，
		//   UDP 侧会稍晚才被读到。主循环 1 kHz、单包几十字节，实测影响仅在毫秒级，
		//   对参数读写/遥测完全够用。真出现"QGC 命令超时"再改成每 N 轮强制读一次 UDP。
		if (i > 0 || !udpStarted) return i;
	}

	// UDP 模式（纯 ESP-NOW 时 udpStarted=false，直接返回 0，不碰 socket）
	if (!udpStarted) return 0;
	if (WiFi.softAPgetStationNum() == 0 && !WiFi.isConnected()) return 0;
	udp.parsePacket();
	if (udp.remoteIP()) udpRemoteIP = udp.remoteIP();
	return udp.read(buf, len);
}

void printWiFiInfo() {
	// 【判据用 wifiMode 而不是 `if (espnow)`】
	//   `espnow` 的 operator bool 返回的是"默认 peer 是否登记成功"（库源码
	//   ESP_NOW_Peer::operator bool() => added）。若 ESP-NOW 初始化失败，
	//   用 `if (espnow)` 会把模式误显示成 AP，掩盖故障。
	//   这里按配置的模式走，初始化成功与否由下面的 TXT_WIFI_NATIVE 单独反映。
	if (wifiMode == W_ESPNOW || wifiMode == W_BOTH) {
		print(wifiMode == W_BOTH ? TXT_WIFI_MODE_BOTH : TXT_WIFI_MODE_ESPNOW);
		print(TXT_WIFI_VERSION, ESP_NOW.getVersion());
		print(TXT_WIFI_MAXLEN, ESP_NOW.getMaxDataLen());
		// MAC 改打 **STA MAC**（eFuse 出厂值）：ESP-NOW 实际用的就是它，
		// 也是热点名后缀的来源。以前打 AP MAC，与 `ap` 自动命名的规则对不上。
		uint8_t mac[6];
		esp_read_mac(mac, ESP_MAC_WIFI_STA);
		char macStr[18];
		snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
			mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		print(TXT_WIFI_MAC, macStr);
		// 【空指针防御】`espnow.addr()` 在 peer 未设置地址时返回 nullptr，
		// 而 `MacAddress(nullptr)` 内部会按 6 字节 memcpy ⇒ 直接空指针崩溃。
		// 旧写法靠 `if (espnow)`（= peer 已登记）挡住了这条路径；
		// 现在判据换成 wifiMode，初始化失败时也会走到这里，必须自己挡一下。
		const uint8_t *peerAddr = espnow.addr();
		if (peerAddr != nullptr) {
			print(TXT_WIFI_PEER, MacAddress(peerAddr).toString().c_str());
		}
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
		// 【共存模式】把 AP 侧状态一并打出来，现场直接判断"地面站到底连没连上"：
		//   客户端=0 → 手机/QGC 根本没连上热点（查 SSID/密码/信号）
		//   客户端>0 但 QGC 无遥测 → 连上了热点但 UDP 没通（查 14550 端口/防火墙）
		if (udpStarted) {
			print(TXT_WIFI_SSID, WiFi.softAPSSID().c_str());
			print(TXT_WIFI_CLIENTS, WiFi.softAPgetStationNum());
			print(TXT_WIFI_IP, WiFi.softAPIP().toString().c_str());
			print(TXT_WIFI_REMOTE_IP, udpRemoteIP.toString().c_str());
		}
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
	// 【写入端防呆（2026-11-05）】
	//   原来这里只判 `strlen(second) >= 8`，于是照文档敲 `ap <热点名> <密码>` 的人
	//   会把占位符本身写进 NVS：`<密码>` 的 UTF-8 正好 8 字节，**刚好卡在门槛上**，
	//   于是热点密码变成了这串中文字节，用户输入任何正常密码都连不上。
	//   现在改用 isUsableApName() / isUsableWifiPass() 统一校验：
	//   长度、可打印 ASCII、尖括号占位符全挡住，并在拒绝时明确告知原因，
	//   让用户知道是"参数格式不对"，而不是以为固件有 bug。
	if (mode == W_AP) {
		if (!isUsableApName(first) || !isUsableWifiPass(second)) {
			print(TXT_WIFI_ARG_INVALID);
			return;
		}
		storage.putString("WIFI_AP_SSID", first);
		storage.putString("WIFI_AP_PASS", second);
	} else if (mode == W_STA) {
		if (!isUsableApName(first) || !isUsableWifiPass(second)) {
			print(TXT_WIFI_ARG_INVALID);
			return;
		}
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
	// 【命令名保持不变】ap / sta / espnow / off 四个老命令一字不改，老文档、老习惯、售后
	//   说明全部继续有效；只新增 both 作为"遥控 + 地面站同时在线"的显式入口。
	//   默认值虽然已经是 both，但仍保留这个命令：用户排查完干扰后要手动切回来时用得上。
	if (mode == "ap") {
		wifiMode = W_AP;
	} else if (mode == "sta") {
		wifiMode = W_STA;
	} else if (mode == "espnow") {
		wifiMode = W_ESPNOW;
	} else if (mode == "both") {
		wifiMode = W_BOTH;
	} else if (mode == "off") {
		wifiMode = W_DISABLED;
	} else {
		print(TXT_WIFI_MODE_INVALID);
		return;
	}
	static const char *modes[] = {"关闭", "热点(AP)", "客户端(STA)", "ESP-NOW", "ESP-NOW+热点(共存)"};
	const int modeCount = sizeof(modes) / sizeof(modes[0]);
	// 越界保护：NVS 里若存过异常值（换固件/手工写坏），直接索引会读到野指针
	print(TXT_WIFI_MODE_SET, (wifiMode >= 0 && wifiMode < modeCount) ? modes[wifiMode] : "?");
}
