#include "WiFiManager.h"
#include "../DisplayManager/DisplayManager.h"

WiFiManager::WiFiManager() :
    mqttClient(nullptr),
    lastMqttConnectAttempt(0),
    lastWiFiConnectAttempt(0),
    wifiInitialized(false),
    isConnecting(false),
    connectionStatusDisplayed(false) {
}

WiFiManager::~WiFiManager() {
    if (mqttClient) {
        delete mqttClient;
        mqttClient = nullptr;
    }
}

void WiFiManager::connectWiFi() {
    if (WiFi.status() == WL_CONNECTED) {
        isConnecting = false;
        connectionStatusDisplayed = false;
        return;
    }

    isConnecting = true;

    if (!connectionStatusDisplayed) {
        Serial.println("正在连接WiFi...");
        displayManager.setTextColor(displayManager.getWhiteColor());
        displayManager.setTextSize(1);
        displayManager.displayText("正在连接WiFi", false);
        connectionStatusDisplayed = true;
    }

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        yield();
        esp_task_wdt_reset();  // 喂狗，防止看门狗复位
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\nWiFi连接成功! IP: %s\n", WiFi.localIP().toString().c_str());
        displayManager.displayText("WiFi连接成功", false);
        isConnecting = false;
        connectionStatusDisplayed = false;
    } else {
        Serial.println("\nWiFi连接失败");
        connectionStatusDisplayed = false;
    }
}

void WiFiManager::init() {
    Serial.println("WiFi正在初始化...");

    // 初始化 MQTT 客户端
    mqttClient = new PubSubClient(wifiClient);
    mqttClient->setServer(MQTT_SERVER, MQTT_PORT);
    mqttClient->setCallback([this](char* topic, byte* payload, unsigned int length) {
        this->parseAndDisplay((const char*)payload);
    });

    // 首次连接 WiFi
    connectWiFi();
    wifiInitialized = true;
}

void WiFiManager::update() {
    // 检查 WiFi 连接状态，如果断开则尝试重连
    if (wifiInitialized && WiFi.status() != WL_CONNECTED) {
        unsigned long now = millis();
        if (now - lastWiFiConnectAttempt >= wifiReconnectInterval) {
            lastWiFiConnectAttempt = now;
            Serial.println("WiFi断开连接，正在尝试重新连接...");
            connectWiFi();
        }
    }

    // 检查 MQTT 连接（需要在 WiFi 连接成功后）
    if (WiFi.status() == WL_CONNECTED) {
        if (!mqttClient->connected()) {
            unsigned long now = millis();
            if (now - lastMqttConnectAttempt >= mqttReconnectInterval) {
                lastMqttConnectAttempt = now;
                Serial.println("Attempting MQTT connection...");
                String clientId = "ESP32-" + String(ESP.getEfuseMac(), HEX);

                if (mqttClient->connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD)) {
                    Serial.println("MQTT服务器已连接");
                    mqttClient->subscribe(MQTT_TOPIC);
                    Serial.printf("已订阅主题: %s\n", MQTT_TOPIC);
                    displayManager.setTextSize(1);
                    displayManager.displayText("MQTT服务器已连接", false);
                } else {
                    Serial.printf("MQTT连接失败，错误码：%d\n", mqttClient->state());
                }
            }
        } else {
            mqttClient->loop();
        }
    }
}

bool WiFiManager::isMqttConnected() {
    return mqttClient && mqttClient->connected();
}

void WiFiManager::parseAndDisplay(const char* payload) {
    Serial.printf("Received MQTT message: %s\n", payload);

    // 解析 JSON
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
        Serial.printf("JSON parsing failed: %s\n", error.c_str());
        return;
    }

    // 提取字段
    const char* text = doc["text"];
    bool scrollMode = doc["scroll_mode"] | false;
    int fontSize = doc["font_size"] | 1;
    const char* colorHex = doc["color"] | "#FF0000";
    int scrollLine = doc["scroll_line"] | 1;
    int scrollSpeed = doc["scroll_speed"] | 1;

    // 解析颜色
    uint16_t color = displayManager.getRedColor(); // 默认红色
    if (strlen(colorHex) == 7 && colorHex[0] == '#') {
        int r = strtol(colorHex + 1, nullptr, 16) >> 16 & 0xFF;
        int g = strtol(colorHex + 3, nullptr, 16) >> 8 & 0xFF;
        int b = strtol(colorHex + 5, nullptr, 16) & 0xFF;
        color = (r & 0xF8) << 8 | (g & 0xFC) << 3 | (b >> 3);
    }

    Serial.printf("Text: %s, Scroll: %d, Size: %d, Line: %d, Speed: %d\n", text, scrollMode, fontSize, scrollLine, scrollSpeed);

    // 应用设置
    displayManager.setTextSize(fontSize);
    displayManager.setTextColor(color);
    displayManager.setTextScrollSpeed(scrollSpeed);

    if (scrollMode) {
        displayManager.displayText(text, true, scrollLine);
    } else {
        displayManager.displayText(text, false, scrollLine);
    }
}

WiFiManager wifiManager;
