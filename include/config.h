#ifndef CONFIG_H
#define CONFIG_H

#define DEBUG_MODE 0     // 0: 关闭调试模式, 1: 开启调试模式
#define LED_SIZE 1       // 0: 64x32, 1: 64x64

// 调试日志宏
#if(DEBUG_MODE == 1)
    #define DEBUG_LOG(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
    #define DEBUG_LOG_INIT(x) Serial.begin(x)
#else
    #define DEBUG_LOG(fmt, ...)
    #define DEBUG_LOG_INIT(x)
#endif  

// HUB75E LED矩阵配置
#if(LED_SIZE == 0)
    #define PANEL_RES_X 64      // LED矩阵宽度
    #define PANEL_RES_Y 32      // LED矩阵高度
    #define PANEL_CHAIN 1       // 矩阵链长度
#elif(LED_SIZE == 1)
    #define PANEL_RES_X 64      // LED矩阵宽度
    #define PANEL_RES_Y 64      // LED矩阵高度
    #define PANEL_CHAIN 1       // 矩阵链长度
#endif

// GPIO引脚定义
#if(LED_SIZE == 0)
    #define R1_PIN 18
    #define G1_PIN 38
    #define B1_PIN 17
    #define R2_PIN 16
    #define G2_PIN 39
    #define B2_PIN 15
    #define A_PIN 7
    #define B_PIN 41
    #define C_PIN 6
    #define D_PIN 2
    #define E_PIN -1 // required for 1/32 scan panels, like 64x64px. Any available pin would do, i.e.
    #define LAT_PIN 1
    #define OE_PIN 4
    #define CLK_PIN 5
#elif(LED_SIZE == 1)
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
#endif

#define DEFAULT_BRIGHTNESS 128  // 默认亮度 (0-255)

// ============================================================================
// 滚动文本配置
// ============================================================================

// 滚动像素偏移（每次更新移动的像素数）
#define SCROLL_OFFSET_LEFT -1       // 向左滚动
#define SCROLL_OFFSET_RIGHT 1       // 向右滚动

// 滚动速度说明：
// - 速度等级：1=慢(每3帧48ms移动1像素)，2=中(每2帧32ms移动1像素)，3=快(每帧16ms移动1像素)
// - 采用帧计数机制，确保滚动更新与帧渲染完全同步，避免时间不同步导致的跳跃感
// - 渲染频率：固定60fps，每帧16ms

// ============================================================================
// WiFi 和 MQTT 配置
// ============================================================================
#define AP_SSID "ESP32-LED-Matrix"

#define MQTT_SERVER "zheng221.xyz"
#define MQTT_PORT 1883
#if(LED_SIZE == 0)
    #define MQTT_USERNAME "64x32esp32"
#elif(LED_SIZE == 1)
    #define MQTT_USERNAME "64x64esp32"
#endif
#define MQTT_PASSWORD "123456"

// MQTT主题由设备MAC地址动态生成，格式为：LED/{mac}/...
// 例如：LED/246f28a58ec4/Text, LED/246f28a58ec4/Clear 等

#define MAX_MQTT_PAYLOAD_SIZE 12800  // MQTT消息最大载荷大小（字节）

#endif
