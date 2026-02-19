#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <Preferences.h>

// 前置声明，减少头文件依赖，加快编译
class WebServer;
class DNSServer;
class PubSubClient;


class WiFiManager {
private:
    // WiFi和MQTT客户端
    WiFiClient wifiClient;                    // MQTT客户端的底层WiFi连接
    PubSubClient *mqttClient;                // MQTT客户端指针，用于接收和发送MQTT消息

    // 配网相关
    WebServer *webServer;                    // HTTP服务器，用于提供配网页面
    DNSServer *dnsServer;                    // DNS服务器，用于Captive Portal功能（劫持DNS请求）

    // 配置存储
    Preferences preferences;                  // ESP32的Preferences库，用于持久化存储WiFi配置

    // MQTT重连计时
    unsigned long lastMqttConnectAttempt;     // 上次尝试连接MQTT服务器的时间戳
    const unsigned long mqttReconnectInterval = 2000;  // MQTT重连间隔（毫秒）

    // WiFi重连计时
    unsigned long lastWiFiConnectAttempt;     // 上次尝试重连WiFi的时间戳（用于断线重连）
    const unsigned long wifiReconnectInterval = 2000;  // WiFi重连间隔（毫秒）

    // WiFi状态标志
    bool wifiInitialized;                     // WiFi模块是否已完成初始化
    bool isConnecting;                       // 当前是否正在连接WiFi（用于超时检测）
    bool connectionStatusDisplayed;           // 连接状态信息是否已显示到LED屏幕（避免重复显示）

    // 配网模式
    bool isConfigMode;                       // 当前是否处于配网模式（AP + WebServer）

    // 首次连接超时检测
    unsigned long connectStartTime;            // WiFi连接开始的时间戳
    const unsigned long connectTimeout = 20000;  // WiFi连接超时时间（毫秒），超时后进入配网模式

    // DNS配置
    const byte DNS_PORT = 53;                // DNS服务器端口号

    // 私有方法
    void parseAndDisplayText(const char* payload);    // 解析MQTT消息并显示到LED屏幕
    void connectWiFi();                          // 连接WiFi（使用已保存的配置）
    void startConfigMode();                      // 启动AP配网模式
    void handleRoot();                           // 处理根路径请求（返回配网页面）
    void handleScan();                           // 处理WiFi扫描请求
    void handleSave();                           // 处理WiFi配置保存请求
    void handleNotFound();                        // 处理未知请求（用于Captive Portal重定向）
    void sendResponse(int code, const char* type, const char* content);  // 发送HTTP响应

public:
    WiFiManager();
    ~WiFiManager();

    void init();                                   // 初始化WiFi管理器
    void loop();                                 // 在loop中调用，处理WiFi/MQTT连接和配网
    bool isMqttConnected();                        // 检查MQTT是否已连接
    bool isWiFiConnecting() const { return isConnecting; }       // 检查是否正在连接WiFi
    bool isInConfigMode() const { return isConfigMode; }         // 检查是否处于配网模式
};

extern WiFiManager wifiManager;

#endif // WIFI_MANAGER_H
