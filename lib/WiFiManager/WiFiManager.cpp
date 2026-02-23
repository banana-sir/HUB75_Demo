#include "WiFiManager.h"
#include "../DisplayManager/DisplayManager.h"
#include <WebServer.h>
#include <DNSServer.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "esp_task_wdt.h"

// 外部声明：跨核访问保护
extern portMUX_TYPE displayMux;

WiFiManager::WiFiManager() :
    mqttClient(nullptr),
    webServer(nullptr),
    dnsServer(nullptr),
    lastMqttConnectAttempt(0),
    lastWiFiConnectAttempt(0),
    mqttFailCount(0),
    wifiInitialized(false),
    isConnecting(false),
    connectionStatusDisplayed(false),
    isConfigMode(false),
    connectStartTime(0),
    mqttWasConnected(false) {
}

WiFiManager::~WiFiManager() {
    if (mqttClient) {
        delete mqttClient;
        mqttClient = nullptr;
    }
    if (webServer) {
        delete webServer;
        webServer = nullptr;
    }
    if (dnsServer) {
        delete dnsServer;
        dnsServer = nullptr;
    }
}

void WiFiManager::connectWiFi() {
    // 强制设置连接中状态（用于所有WiFi连接场景：首次连接、断线重连、MQTT失败触发重连）
    isConnecting = true;
    connectStartTime = millis();

    // 显示连接提示（如果是第一次显示）
    if (!connectionStatusDisplayed) {
        Serial.println("正在连接WiFi...");
        displayManager.setTextSize(1);
        displayManager.displayText("正在连接WiFi", false, displayManager.whiteColor);
        connectionStatusDisplayed = true;
    }

    // 从Preferences读取保存的WiFi信息
    String ssid = preferences.getString("wifi_ssid", "");
    String password = preferences.getString("wifi_password", "");

    if (ssid.length() > 0) {
        Serial.printf("尝试连接保存的WiFi: %s\n", ssid.c_str());
        WiFi.begin(ssid.c_str(), password.c_str());
    } else {
        Serial.println("未找到保存的WiFi信息，进入配网模式");
        isConnecting = false;
        connectionStatusDisplayed = false;
        startConfigMode();
        return;
    }
}

void WiFiManager::reconnectWiFi() {
    Serial.println("WiFi断开连接，正在尝试重新连接...");

    // 断开现有连接
    WiFi.disconnect();

    // 等待WiFi完全断开（最多等待2秒）
    int waitCount = 0;
    while (WiFi.status() == WL_CONNECTED && waitCount < 40) {
        delay(50);
        waitCount++;
    }

    // 重置连接状态标志
    connectionStatusDisplayed = false;

    // 开始重新连接
    connectWiFi();
}

void WiFiManager::init() {
    Serial.println("WiFi正在初始化...");

    // 初始化 Preferences (用于保存WiFi配置)
    preferences.begin("wifi_config", false);

    // 生成并保存 MQTT ClientId
    clientId = "ESP32/" + String(PANEL_RES_X) + 'x' + String(PANEL_RES_Y) + '/' + String(ESP.getEfuseMac(), HEX);
    Serial.printf("MQTT ClientId: %s\n", clientId.c_str());

    // 初始化 MQTT 客户端
    if (!mqttClient) {
        mqttClient = new PubSubClient(wifiClient);
        mqttClient->setServer(MQTT_SERVER, MQTT_PORT);
        mqttClient->setBufferSize(MAX_MQTT_PAYLOAD_SIZE);
        mqttClient->setCallback([this](char* topic, byte* payload, unsigned int length) {
            this->mqttCallback(topic, payload, length);
        });
    }

    // 首次连接 WiFi
    connectWiFi();
    wifiInitialized = true;
}

void WiFiManager::loop() {
    // 处理配网模式下的Web服务器和DNS服务器
    if (isConfigMode && webServer) {
        webServer->handleClient();
        if (dnsServer) {
            dnsServer->processNextRequest();
        }
        // 配网模式下持续喂狗
        esp_task_wdt_reset();
        return;
    }

    // ========== WiFi连接管理 ==========
    // wifiInitialized: WiFi模块是否已初始化（调用过connectWiFi至少一次）
    // isConnecting: WiFi是否正在连接中（用于超时检测和阻止MQTT连接）
    // 只有WiFi已初始化且当前未在连接、且连接断开时，才尝试重连
    if (wifiInitialized && WiFi.status() != WL_CONNECTED && !isConnecting) {
        unsigned long now = millis();
        if (now - lastWiFiConnectAttempt >= wifiReconnectInterval) {
            lastWiFiConnectAttempt = now;
            reconnectWiFi();
        }
    }

    // 检查连接是否成功或超时
    if (isConnecting) {
        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("\nWiFi连接成功! IP: %s\n", WiFi.localIP().toString().c_str());
            displayManager.setTextSize(1);
            displayManager.displayText("WiFi连接成功", false, displayManager.whiteColor);
            isConnecting = false;
            connectionStatusDisplayed = false;
            mqttFailCount = 0;  // WiFi连接成功，重置MQTT失败计数
        } else if (millis() - connectStartTime >= connectTimeout) {
            Serial.println("\nWiFi连接超时，进入配网模式");
            isConnecting = false;
            connectionStatusDisplayed = false;
            startConfigMode();
        }
    }

    // ========== MQTT连接管理 ==========
    // 只有WiFi已连接且不在连接过程中（isConnecting=false）时才处理MQTT
    if (WiFi.status() == WL_CONNECTED && !isConnecting) {
        if (!mqttClient->connected()) {
            unsigned long now = millis();
            if (now - lastMqttConnectAttempt >= mqttReconnectInterval) {
                lastMqttConnectAttempt = now;
                Serial.printf("Attempting MQTT connection to %s:%d... (失败次数: %d/%d)\n",
                             MQTT_SERVER, MQTT_PORT, mqttFailCount, maxMqttFailCount);

                if (mqttClient->connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD)) {
                    Serial.println("MQTT服务器已连接");
                    mqttClient->subscribe(MQTT_TOPIC_TEXT);
                    Serial.printf("已订阅主题: %s\n", MQTT_TOPIC_TEXT);
                    mqttClient->subscribe(MQTT_TOPIC_CLEAR);
                    Serial.printf("已订阅主题: %s\n", MQTT_TOPIC_CLEAR);
                    mqttClient->subscribe(MQTT_TOPIC_BRIGHTNESS);
                    Serial.printf("已订阅主题: %s\n", MQTT_TOPIC_BRIGHTNESS);
                    mqttClient->subscribe(MQTT_TOPIC_IMAGE);
                    Serial.printf("已订阅主题: %s\n", MQTT_TOPIC_IMAGE);
                    displayManager.displayText("MQTT服务器已连接", false, displayManager.whiteColor);

                    // 连接成功，重置失败计数
                    mqttFailCount = 0;
                    mqttWasConnected = true;
                } else {
                    int state = mqttClient->state();
                    mqttFailCount++;
                    Serial.printf("MQTT连接失败，状态: %d\n", state);

                    // 如果失败5次，强制触发WiFi重连（可能是DNS或网络问题）
                    if (mqttFailCount >= maxMqttFailCount) {
                        Serial.printf("MQTT连续失败 %d 次，触发WiFi重连\n", maxMqttFailCount);
                        mqttFailCount = 0;
                        reconnectWiFi();
                    }
                }
            }
        } else {
            mqttClient->loop();
        }
    } else {
        // WiFi断开时，重置MQTT状态
        if (mqttWasConnected) {
            Serial.println("WiFi断开，MQTT连接已断开");
            mqttWasConnected = false;
            mqttFailCount = 0;
        }
    }
}

bool WiFiManager::isMqttConnected() {
    return mqttClient && mqttClient->connected();
}

void WiFiManager::mqttCallback(char* topic, byte* payload, unsigned int length) {
    String topicStr = String(topic);

    String msg;
    msg.reserve(length + 1);
    for (unsigned int i = 0; i < length; i++) {
        msg += (char)payload[i];
    }
    msg.trim();

    if (topicStr.equals(MQTT_TOPIC_TEXT)) {
        this->parseAndDisplayText(msg.c_str());
    } else if (topicStr.equals(MQTT_TOPIC_CLEAR)) {
        // 先检查target字段
        if (!checkTarget(msg.c_str())) {
            return;
        }

        // 解析 JSON
        StaticJsonDocument<64> doc;
        DeserializationError error = deserializeJson(doc, msg.c_str());

        if (error) {
            Serial.printf("Clear消息JSON解析失败: %s\n", error.c_str());
            return;
        }

        bool clear = doc["clear"] | false;
        if (clear) {
            displayManager.clearAll();
        }
    } else if (topicStr.equals(MQTT_TOPIC_BRIGHTNESS)) {
        // 先检查target字段
        if (!checkTarget(msg.c_str())) {
            return;
        }

        // 解析 JSON
        StaticJsonDocument<64> doc;
        DeserializationError error = deserializeJson(doc, msg.c_str());

        if (error) {
            Serial.printf("Brightness消息JSON解析失败: %s\n", error.c_str());
            return;
        }

        int b = doc["brightness"] | 128;
        if (b < 0) b = 0;
        if (b > 255) b = 255;
        displayManager.setBrightness((uint8_t)b);
    } else if (topicStr.equals(MQTT_TOPIC_IMAGE)) {
        // 图片消息直接传递原始 payload，避免大 JSON 文档在栈上分配
        this->parseAndDisplayImage(payload, length);
    } else {
        Serial.printf("未知主题: %s\n", topic);
    }
}

bool WiFiManager::checkTarget(const char* payload) {
    // 使用字符串查找来快速提取target字段，避免完整JSON解析
    const char* targetStart = strstr(payload, "\"target\":");
    if (targetStart == nullptr) {
        Serial.println("消息缺少 target 字段");
        return false;
    }

    // 跳过 "target":
    targetStart += 9;

    // 跳过冒号后的空格（如果有）
    while (*targetStart == ' ') {
        targetStart++;
    }

    // 查找引号开始
    if (*targetStart != '"') {
        Serial.println("target 字段格式错误");
        return false;
    }
    const char* targetValueStart = targetStart + 1;

    // 查找引号结束
    const char* targetValueEnd = strchr(targetValueStart, '"');
    if (targetValueEnd == nullptr) {
        Serial.println("target 字段格式错误");
        return false;
    }

    // 提取 target 值
    size_t targetLength = targetValueEnd - targetValueStart;
    String targetStr = String(targetValueStart, targetLength);

    // 检查目标是否匹配当前设备或通配符（根据分辨率动态生成）
    String allDevicesTarget = "All_" + String(PANEL_RES_X) + "x" + String(PANEL_RES_Y);

    if (!targetStr.equals(clientId) && !targetStr.equals(allDevicesTarget)) {
        Serial.printf("目标不匹配: 收到=%s, 当前ClientId=%s, 通配符=%s\n", targetStr.c_str(), clientId.c_str(), allDevicesTarget.c_str());
        return false;
    }

    Serial.printf("目标匹配: %s\n", targetStr.c_str());
    return true;
}

void WiFiManager::parseAndDisplayText(const char* payload) {
    Serial.printf("收到文本MQTT消息\n");

    // 先检查target字段，提高效率
    if (!checkTarget(payload)) {
        return;
    }

    // 解析 JSON
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
        Serial.printf("JSON解析失败: %s\n", error.c_str());
        return;
    }

    // 提取字段
    const char* text = doc["text"];
    bool scrollMode = doc["scroll_mode"] | false;
    int fontSize = doc["font_size"] | 1;
    const char* colorHex = doc["color"] | "#FFFFFF";
    int line = doc["line"] | 1;
    bool wrap = doc["wrap"] | true; // 静态文本是否自动换行，默认 true
    int scrollSpeed = doc["scroll_speed"] | 1;
    int scrollDirection = doc["scroll_direction"] | 0;  // 0=向左，1=向右


    // 解析颜色
    uint16_t color = displayManager.whiteColor; // 默认白色
    if (strlen(colorHex) == 7 && colorHex[0] == '#') {
        int r = strtol(colorHex + 1, nullptr, 16) >> 16 & 0xFF;
        int g = strtol(colorHex + 3, nullptr, 16) >> 8 & 0xFF;
        int b = strtol(colorHex + 5, nullptr, 16) & 0xFF;
        color = (r & 0xF8) << 8 | (g & 0xFC) << 3 | (b >> 3);
    }

    Serial.printf("Text: %s, Scroll: %d, Color: %d, Size: %d, Line: %d, Wrap: %d, Speed: %d, Direction: %d\n", text, scrollMode, color, fontSize, line, wrap, scrollSpeed, scrollDirection);

    // 应用设置
    displayManager.setTextSize(fontSize);

    // 转换方向：0=向左(SCROLL_LEFT)，其他=向右(SCROLL_RIGHT)
    DisplayManager::ScrollDirection direction = (scrollDirection == 0) ? DisplayManager::SCROLL_LEFT : DisplayManager::SCROLL_RIGHT;

    // 传入参数并显示
    displayManager.displayText(text, scrollMode, color, line, wrap, scrollSpeed, direction);

}

void WiFiManager::parseAndDisplayImage(byte* payload, unsigned int length) {
    Serial.printf("收到图片MQTT消息，大小: %d 字节\n", length);

    // 检查消息大小
    if (length > MAX_MQTT_PAYLOAD_SIZE) {
        Serial.printf("警告：MQTT消息过大 %d 字节，超出限制 %d 字节\n", length, MAX_MQTT_PAYLOAD_SIZE);
        return;
    }

    // 先检查target字段，提高效率
    const char* payloadStr = (const char*)payload;
    if (!checkTarget(payloadStr)) {
        return;
    }

    // 在原始 payload 中查找 image_base64 字段的起始位置
    const char* imageBase64Start = strstr(payloadStr, "\"image_base64\":\"");

    if (imageBase64Start == nullptr) {
        Serial.println("图片消息缺少 image_base64 字段");
        return;
    }

    // 跳过字段名称和引号
    imageBase64Start += 16;  // 跳过 "\"image_base64\":\""

    // 查找结束引号
    const char* imageBase64End = strchr(imageBase64Start, '"');

    if (imageBase64End == nullptr) {
        Serial.println("图片消息的 image_base64 字段格式错误");
        return;
    }

    // 计算base64数据长度
    int imageLength = imageBase64End - imageBase64Start;
    Serial.printf("收到图片数据，base64长度: %d 字节\n", imageLength);

    // 检查base64数据大小是否超过限制
    if (imageLength > MAX_MQTT_PAYLOAD_SIZE) {
        Serial.printf("警告：base64数据过大 %d 字节，超出限制 %d 字节\n", imageLength, MAX_MQTT_PAYLOAD_SIZE);
        return;
    }

    // 调用 displayImage 显示图片
    displayManager.displayImage(imageBase64Start, imageLength);
}

void WiFiManager::startConfigMode() {
    Serial.println("启动AP配网模式...");

    // 停止现有WiFi连接
    WiFi.disconnect();
    delay(100);

    // 配置AP模式
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
    WiFi.softAP(AP_SSID);

    Serial.printf("AP已启动: %s\n", AP_SSID);
    Serial.printf("AP IP: %s\n", WiFi.softAPIP().toString().c_str());
    Serial.printf("AP网关: %s\n", WiFi.softAPIP().toString().c_str());
    Serial.printf("AP子网掩码: 255.255.255.0\n");

    // 显示配网信息到LED屏幕
    displayManager.setTextSize(1);
    displayManager.displayText("配网模式", false, displayManager.whiteColor);
    
    String apInfo = "请连接热点：" + String(AP_SSID);
    displayManager.displayText(apInfo.c_str(), true, displayManager.whiteColor, 2);

    // 初始化DNS服务器（Captive Portal）
    if (!dnsServer) {
        dnsServer = new DNSServer();
    }
    dnsServer->start(DNS_PORT, "*", WiFi.softAPIP());
    Serial.println("DNS服务器已启动，监听端口53");

    // 初始化Web服务器
    if (!webServer) {
        webServer = new WebServer(80);
    }

    // 设置路由
    webServer->on("/", std::bind(&WiFiManager::handleRoot, this));
    webServer->on("/scan", std::bind(&WiFiManager::handleScan, this));
    webServer->on("/save", std::bind(&WiFiManager::handleSave, this));
    webServer->on("/generate_204", std::bind(&WiFiManager::handleRoot, this));  // Android检测
    webServer->on("/fwlink", std::bind(&WiFiManager::handleRoot, this));      // Windows检测
    webServer->on("/connecttest", std::bind(&WiFiManager::handleRoot, this));  // Windows检测
    webServer->on("/hotspot-detect", std::bind(&WiFiManager::handleRoot, this)); // iOS/macOS检测
    webServer->onNotFound(std::bind(&WiFiManager::handleNotFound, this));

    // 启用CORS
    webServer->enableCORS(true);

    webServer->begin();
    isConfigMode = true;

    Serial.println("Web服务器已启动，监听端口80");
    Serial.println("支持Captive Portal自动跳转");
    Serial.println("访问地址: http://192.168.4.1");
}

void WiFiManager::handleRoot() {
    // 添加Captive Portal相关HTTP头
    webServer->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    webServer->sendHeader("Pragma", "no-cache");
    webServer->sendHeader("Expires", "-1");

    String html = "<!DOCTYPE html><html><head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>LED点阵屏配网</title>";
    html += "<style>";
    html += "*{margin:0;padding:0;box-sizing:border-box}";
    html += "body{font-family:'Segoe UI',Arial,sans-serif;background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);min-height:100vh;padding:20px}";
    html += ".container{max-width:500px;margin:40px auto;background:white;border-radius:20px;box-shadow:0 20px 60px rgba(0,0,0,0.3);overflow:hidden}";
    html += ".header{background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);color:white;padding:30px;text-align:center}";
    html += ".header h1{font-size:28px;margin-bottom:10px}";
    html += ".header p{opacity:0.9;font-size:14px}";
    html += ".content{padding:30px}";
    html += ".form-group{margin-bottom:20px}";
    html += "label{display:block;margin-bottom:8px;color:#333;font-weight:600;font-size:14px}";
    html += "input,select{width:100%;padding:12px 15px;border:2px solid #e0e0e0;border-radius:10px;font-size:14px;transition:all 0.3s}";
    html += "input:focus,select:focus{outline:none;border-color:#667eea;box-shadow:0 0 0 3px rgba(102,126,234,0.1)}";
    html += ".btn{width:100%;padding:14px;border:none;border-radius:10px;font-size:16px;font-weight:600;cursor:pointer;transition:all 0.3s}";
    html += ".btn-primary{background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);color:white}";
    html += ".btn-primary:hover{transform:translateY(-2px);box-shadow:0 10px 20px rgba(102,126,234,0.3)}";
    html += ".btn-secondary{background:#f0f0f0;color:#333;margin-top:10px}";
    html += ".btn-secondary:hover{background:#e0e0e0}";
    html += "#wifiList{max-height:300px;overflow-y:auto;border:2px solid #e0e0e0;border-radius:10px;margin-top:10px}";
    html += ".wifi-item{padding:12px 15px;border-bottom:1px solid #eee;cursor:pointer;transition:all 0.2s}";
    html += ".wifi-item:hover{background:#f5f5f5}";
    html += ".wifi-item:last-child{border-bottom:none}";
    html += ".wifi-name{font-weight:600;color:#333;margin-bottom:4px}";
    html += ".wifi-info{font-size:12px;color:#999}";
    html += ".wifi-strength{float:right;color:#4CAF50}";
    html += ".loading{text-align:center;padding:20px;color:#999}";
    html += ".status{padding:15px;border-radius:10px;margin-bottom:20px;text-align:center;font-size:14px}";
    html += ".status.success{background:#d4edda;color:#155724}";
    html += ".status.error{background:#f8d7da;color:#721c24}";
    html += ".status.info{background:#d1ecf1;color:#0c5460}";
    html += "</style></head><body>";
    html += "<div class='container'>";
    html += "<div class='header'>";
    html += "<h1>📡 LED点阵屏配网</h1>";
    html += "<p>请选择或输入您的WiFi信息</p>";
    html += "</div>";
    html += "<div class='content'>";

    // 检查连接状态
    if (WiFi.status() == WL_CONNECTED) {
        html += "<div class='status success'>";
        html += "✅ WiFi连接成功！IP: " + WiFi.localIP().toString();
        html += "</div>";
    }

    html += "<div class='form-group'>";
    html += "<label>📶 可用WiFi网络</label>";
    html += "<button type='button' class='btn btn-secondary' onclick='scanWiFi()'>🔍 扫描WiFi</button>";
    html += "<div id='wifiList'><div class='loading'>点击上方按钮扫描可用网络</div></div>";
    html += "</div>";

    html += "<form id='wifiForm'>";
    html += "<div class='form-group'>";
    html += "<label>📱 WiFi名称 (SSID)</label>";
    html += "<input type='text' id='ssid' name='ssid' placeholder='请输入WiFi名称' required>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>🔐 WiFi密码</label>";
    html += "<input type='password' id='password' name='password' placeholder='请输入WiFi密码'>";
    html += "</div>";
    html += "<button type='submit' class='btn btn-primary'>💾 保存并连接</button>";
    html += "</form>";

    html += "</div></div>";
    html += "<script>";
    html += "function scanWiFi(){var list=document.getElementById('wifiList');list.innerHTML='<div class=\"loading\">正在扫描...</div>';fetch('/scan').then(r=>r.json()).then(d=>{if(d.networks.length>0){var html='';d.networks.forEach(n=>{var s=n.signal_strength>70?'强':(n.signal_strength>50?'中':'弱');html+='<div class=\"wifi-item\" onclick=\"selectWiFi(\\''+n.ssid+'\\')\">';html+='<div class=\"wifi-strength\">'+s+'</div>';html+='<div class=\"wifi-name\">'+n.ssid+'</div>';html+='<div class=\"wifi-info\">信号强度: '+n.signal_strength+'%</div>';html+='</div>'});list.innerHTML=html}else{list.innerHTML='<div class=\"loading\">未找到WiFi网络</div>'}}).catch(e=>{list.innerHTML='<div class=\"loading\">扫描失败，请重试</div>'})}";
    html += "function selectWiFi(ssid){document.getElementById('ssid').value=ssid}";
    html += "document.getElementById('wifiForm').addEventListener('submit',function(e){e.preventDefault();var ssid=document.getElementById('ssid').value;var password=document.getElementById('password').value;fetch('/save',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'ssid='+encodeURIComponent(ssid)+'&password='+encodeURIComponent(password)}).then(r=>r.json()).then(d=>{if(d.success){document.querySelector('.content').innerHTML='<div class=\"status success\">✅ 保存成功！正在连接WiFi...</div><div class=\"status info\">请等待约10秒，然后刷新页面查看连接状态</div>'}else{document.querySelector('.content').innerHTML='<div class=\"status error\">❌ '+d.message+'</div>'}}).catch(e=>{document.querySelector('.content').innerHTML='<div class=\"status error\">❌ 保存失败，请重试</div>'})})";
    html += "</script></body></html>";

    sendResponse(200, "text/html", html.c_str());
}

void WiFiManager::handleScan() {
    Serial.println("扫描WiFi网络...");

    // 扫描期间喂狗
    esp_task_wdt_reset();

    int n = WiFi.scanNetworks();
    Serial.printf("找到 %d 个网络\n", n);

    String json = "{\"networks\":[";

    for (int i = 0; i < n; i++) {
        if (i > 0) json += ",";
        json += "{";
        json += "\"ssid\":\"";
        json += WiFi.SSID(i);
        json += "\",";
        json += "\"signal_strength\":";
        json += WiFi.RSSI(i) + 100;
        json += "}";
    }

    json += "]}";
    sendResponse(200, "application/json", json.c_str());

    // 清除扫描结果
    WiFi.scanDelete();
}

void WiFiManager::handleSave() {
    String ssid = webServer->arg("ssid");
    String password = webServer->arg("password");

    // 保存期间喂狗
    esp_task_wdt_reset();

    if (ssid.length() == 0) {
        String json = "{\"success\":false,\"message\":\"WiFi名称不能为空\"}";
        sendResponse(200, "application/json", json.c_str());
        return;
    }

    Serial.printf("保存WiFi配置: SSID=%s, Password=***\n", ssid.c_str());

    // 保存到Preferences
    preferences.putString("wifi_ssid", ssid);
    preferences.putString("wifi_password", password);

    // 先发送响应
    String json = "{\"success\":true,\"message\":\"配置已保存\"}";
    sendResponse(200, "application/json", json.c_str());

    // 等待响应发送完成
    delay(500);
    esp_task_wdt_reset();

    // 更新显示
    displayManager.setTextSize(1);
    displayManager.displayText("正在连接WiFi", false, displayManager.whiteColor);

    // 关闭配网模式
    isConfigMode = false;
    webServer->stop();
    if (dnsServer) {
        dnsServer->stop();
    }

    // 切换回STA模式并连接
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());

    // 设置连接开始时间
    connectStartTime = millis();
    isConnecting = true;
}

void WiFiManager::handleNotFound() {
    // Captive Portal: 所有未知请求重定向到根路径
    String redirectUrl = "http://" + WiFi.softAPIP().toString();
    webServer->sendHeader("Location", redirectUrl);
    webServer->send(302, "text/plain", "Redirecting to configuration page");
}

void WiFiManager::sendResponse(int code, const char* type, const char* content) {
    webServer->send(code, type, content);
}


