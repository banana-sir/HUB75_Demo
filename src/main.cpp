#include "DisplayManager.h"

DisplayManager displayManager;

void setup() {
  // put your setup code here, to run once:
  displayManager.init();

  Serial.begin(115200);
  Serial.println("Starting...");
  // 配置看门狗，延长超时时间
  esp_task_wdt_init(10, true); // 10秒超时
  esp_task_wdt_add(NULL);
  displayManager.displayText("Hello World!", true, 1);
}


void loop() {
  // 喂狗，防止看门狗复位
  yield();
  esp_task_wdt_reset();

  displayManager.update();
}

