#include "DisplayManager.h"
#include "WiFiManager.h"
#include "esp_task_wdt.h"

DisplayManager displayManager;
WiFiManager wifiManager;

// WiFi/MQTT 任务句柄
TaskHandle_t wifiTaskHandle = NULL;

// WiFi/MQTT 任务函数（运行在 Core 0）
void wifiTaskFunction(void *pvParameters) {
  for (;;) {
    wifiManager.loop();
    vTaskDelay(1);  // 让出 CPU，避免忙等待
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("Starting...");
  Serial.printf("CPU 核数: %d\n", ESP.getChipCores());
  Serial.printf("当前运行在: Core %d\n", xPortGetCoreID());

  // 配置看门狗，延长超时时间
  esp_task_wdt_init(30, true); // 30秒超时
  esp_task_wdt_add(NULL);

  displayManager.init();
  wifiManager.init();

  // 创建 WiFi/MQTT 任务，运行在 Core 0，堆栈大小 16KB
  xTaskCreatePinnedToCore(
    wifiTaskFunction,     // 任务函数
    "WiFiTask",          // 任务名称
    16384,              // 堆栈大小（16KB）
    NULL,               // 参数
    1,                  // 优先级（1=低，2=中等，3=高）
    &wifiTaskHandle,     // 任务句柄
    0                   // Core 0
  );

  Serial.printf("WiFi/MQTT 任务已创建，运行在 Core 0\n");
  Serial.printf("DisplayManager 任务运行在 Core 1\n");
}


void loop() {
  // 喂狗，防止看门狗复位
  esp_task_wdt_reset();

  // 只处理显示相关的任务，运行在 Core 1
  displayManager.loop();
}

