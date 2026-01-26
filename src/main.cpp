#include "DisplayManager.h"
#include "WiFiManager.h"

DisplayManager displayManager;

void setup() {
  // put your setup code here, to run once:

  Serial.begin(115200);
  delay(100);
  Serial.println("Starting...");

  // 配置看门狗，延长超时时间
  esp_task_wdt_init(10, true); // 10秒超时
  esp_task_wdt_add(NULL);

  displayManager.init();
  wifiManager.init();

  
}


void loop() {
  // 喂狗，防止看门狗复位
  yield();
  esp_task_wdt_reset();

  displayManager.update();
  wifiManager.update();
}

