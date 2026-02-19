#ifndef CONFIG_H
#define CONFIG_H

// HUB75E LED矩阵配置
#define PANEL_RES_X 64      // LED矩阵宽度
#define PANEL_RES_Y 64      // LED矩阵高度
#define PANEL_CHAIN 1       // 矩阵链长度

// GPIO引脚定义
#define R1_PIN 6
#define G1_PIN 7
#define B1_PIN 8
#define R2_PIN 9
#define G2_PIN 10
#define B2_PIN 11
#define A_PIN 15
#define B_PIN 16
#define C_PIN 17
#define D_PIN 18
#define E_PIN 47 // required for 1/32 scan panels, like 64x64px. Any available pin would do, i.e. 
#define LAT_PIN 13
#define OE_PIN 14
#define CLK_PIN 12


#define DEFAULT_BRIGHTNESS 128  // 默认亮度 (0-255)



// ============================================================================
// 滚动文本配置
// ============================================================================

// 滚动文本延迟（ms）- 当前各速度档位统一使用相同值
#define SCROLL_TIME_DELAY 30

// 滚动文本像素偏移（负值表示向左滚动）
#define SCROLL_OFFSET_LEFT_LOW -1
#define SCROLL_OFFSET_LEFT_MEDIUM -2
#define SCROLL_OFFSET_LEFT_FAST -3
#define SCROLL_OFFSET_RIGHT_LOW 1
#define SCROLL_OFFSET_RIGHT_MEDIUM 2
#define SCROLL_OFFSET_RIGHT_FAST 3

// ============================================================================
// WiFi 和 MQTT 配置
// ============================================================================
// #define WIFI_SSID "helloworld"
// #define WIFI_PASSWORD "15819652435"
#define AP_SSID "ESP32-LED-Matrix"

#define MQTT_SERVER "zheng241.xyz"
#define MQTT_PORT 1883
#define MQTT_USERNAME "minggo"
#define MQTT_PASSWORD "123456"
#define MQTT_TOPIC_TEXT "LED/Text"
#define MQTT_TOPIC_CLEAR "LED/Clear"
#define MQTT_TOPIC_BRIGHTNESS "LED/Brightness"
#define MQTT_TOPIC_IMAGE "LED/Image"

#define MAX_MQTT_PAYLOAD_SIZE 16800  // MQTT消息最大载荷大小（字节）

#endif
