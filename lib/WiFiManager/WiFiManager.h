#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>


class WiFiManager {
private:
    WiFiClient wifiClient;
    PubSubClient *mqttClient;
    unsigned long lastMqttConnectAttempt;
    const unsigned long mqttReconnectInterval = 5000;
    unsigned long lastWiFiConnectAttempt;
    const unsigned long wifiReconnectInterval = 5000;
    bool wifiInitialized;
    bool isConnecting;
    bool connectionStatusDisplayed;

    void parseAndDisplay(const char* payload);
    void connectWiFi();

public:
    WiFiManager();
    ~WiFiManager();

    void init();
    void update();
    bool isMqttConnected();
    bool isWiFiConnecting() const { return isConnecting; }
};

extern WiFiManager wifiManager;

#endif // WIFI_MANAGER_H
