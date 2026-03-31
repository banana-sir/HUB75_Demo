#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

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
    int mqttFailCount;                        // MQTT连接失败次数
    const int maxMqttFailCount = 5;            // 最大失败次数，超过则触发WiFi重连

    // WiFi重连计时
    unsigned long lastWiFiConnectAttempt;     // 上次尝试重连WiFi的时间戳（用于断线重连）
    const unsigned long wifiReconnectInterval = 2000;  // WiFi重连间隔（毫秒）

    // WiFi状态标志
    bool wifiInitialized;                     // WiFi模块是否已初始化（调用过connectWiFi至少一次）
    bool isConnecting;                       // WiFi是否正在连接中（用于超时检测和阻止MQTT连接）
    bool connectionStatusDisplayed;           // 连接状态信息是否已显示到LED屏幕（避免重复显示）

    // 配网模式
    bool isConfigMode;                       // 当前是否处于配网模式（AP + WebServer）

    // 首次连接超时检测
    unsigned long connectStartTime;            // WiFi连接开始的时间戳
    const unsigned long connectTimeout = 20000;  // WiFi连接超时时间（毫秒），超时后进入配网模式

    // DNS配置
    const byte DNS_PORT = 53;                // DNS服务器端口号

    // MQTT客户端标识
    String clientId;                          // 当前设备的MQTT ClientId

    // 设备特定的MQTT主题
    String topicText;                        // 文本主题: LED/{mac}/Text
    String topicClear;                       // 清屏主题: LED/{mac}/Clear
    String topicBrightness;                  // 亮度主题: LED/{mac}/Brightness
    String topicImage;                       // 图片主题: LED/{mac}/Image

    // MQTT 状态跟踪
    bool mqttWasConnected;                    // MQTT之前是否已连接过

    // MQTT回调消息队列
    struct MqttMessageItem {
        char topic[64];
        byte *payload;
        unsigned int length;
    };
    QueueHandle_t mqttMessageQueue;           // MQTT消息队列
    TaskHandle_t mqttProcessTaskHandle;       // MQTT消息处理任务句柄

    // 私有方法
    bool enqueueMqttMessage(const char* topic, const byte* payload, unsigned int length);
    static void mqttProcessTaskEntry(void *param);
    void mqttProcessTask();
    void mqttCallback(char* topic, byte* payload, unsigned int length);  // MQTT回调函数，处理接收到的消息
    void parseAndDisplayText(const char* payload);    // 解析MQTT文本消息并显示到LED屏幕
    void parseAndDisplayImage(byte* payload, unsigned int length);  // 解析MQTT图片消息并显示到LED屏幕
    void connectWiFi();                          // 连接WiFi（使用已保存的配置）
    void reconnectWiFi();                        // 重新连接WiFi（先断开再连接）
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
