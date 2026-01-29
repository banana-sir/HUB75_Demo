#include "WiFiManager.h"
#include "../DisplayManager/DisplayManager.h"

WiFiManager::WiFiManager() :
    mqttClient(nullptr),
    webServer(nullptr),
    dnsServer(nullptr),
    lastMqttConnectAttempt(0),
    lastWiFiConnectAttempt(0),
    wifiInitialized(false),
    isConnecting(false),
    connectionStatusDisplayed(false),
    isConfigMode(false),
    connectStartTime(0) {
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
    if (WiFi.status() == WL_CONNECTED) {
        isConnecting = false;
        connectionStatusDisplayed = false;
        return;
    }

    isConnecting = true;

    if (!connectionStatusDisplayed) {
        Serial.println("正在连接WiFi...");
        displayManager.setTextColor(displayManager.whiteColor);
        displayManager.setTextSize(1);
        displayManager.displayText("正在连接WiFi", false);
        connectionStatusDisplayed = true;
    }

    // 从Preferences读取保存的WiFi信息
    String ssid = preferences.getString("wifi_ssid", "");
    String password = preferences.getString("wifi_password", "");

    if (ssid.length() > 0) {
        Serial.printf("尝试连接保存的WiFi: %s\n", ssid.c_str());
        WiFi.begin(ssid.c_str(), password.c_str());
        connectStartTime = millis();
    } else {
        Serial.println("未找到保存的WiFi信息，进入配网模式");
        isConnecting = false;
        connectionStatusDisplayed = false;
        startConfigMode();
        return;
    }
}

void WiFiManager::init() {
    Serial.println("WiFi正在初始化...");

    // 初始化 Preferences (用于保存WiFi配置)
    preferences.begin("wifi_config", false);

    // 初始化 MQTT 客户端
    if (!mqttClient) {
        mqttClient = new PubSubClient(wifiClient);
        mqttClient->setServer(MQTT_SERVER, MQTT_PORT);
        mqttClient->setCallback([this](char* topic, byte* payload, unsigned int length) {
            this->parseAndDisplay((const char*)payload);
        });
    }

    // 首次连接 WiFi
    connectWiFi();
    wifiInitialized = true;
}

void WiFiManager::update() {
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

    // 检查 WiFi 连接状态，如果断开则尝试重连
    if (wifiInitialized && WiFi.status() != WL_CONNECTED && !isConnecting) {
        unsigned long now = millis();
        if (now - lastWiFiConnectAttempt >= wifiReconnectInterval) {
            lastWiFiConnectAttempt = now;
            Serial.println("WiFi断开连接，正在尝试重新连接...");
            connectWiFi();
        }
    }

    // 检查首次连接是否超时
    if (isConnecting) {
        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("\nWiFi连接成功! IP: %s\n", WiFi.localIP().toString().c_str());
            displayManager.displayText("WiFi连接成功", false);
            isConnecting = false;
            connectionStatusDisplayed = false;
        } else if (millis() - connectStartTime >= connectTimeout) {
            Serial.println("\nWiFi连接超时，进入配网模式");
            isConnecting = false;
            connectionStatusDisplayed = false;
            startConfigMode();
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
    const char* colorHex = doc["color"] | "#FFFFFF";
    int scrollLine = doc["scroll_line"] | 1;
    int scrollSpeed = doc["scroll_speed"] | 1;

    // 解析颜色
    uint16_t color = displayManager.whiteColor; // 默认白色
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

    displayManager.displayText(text, scrollMode, scrollLine);

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
    displayManager.setTextColor(displayManager.whiteColor);
    displayManager.displayText("配网模式", false);
    
    String apInfo = "请连接热点：" + String(AP_SSID);
    displayManager.setTextScrollSpeed(1);
    displayManager.displayText(apInfo.c_str(), true, 2);

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
    displayManager.setTextColor(displayManager.whiteColor);
    displayManager.displayText("正在连接WiFi", false);

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

WiFiManager wifiManager;

