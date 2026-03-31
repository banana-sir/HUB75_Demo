# MQTT 消息回调优化说明

## 背景

当前实现中，`WiFiManager::mqttCallback()` 直接在 MQTT 回调里解析 JSON 并调用 `displayManager` 显示。这个路径属于耗时操作，尤其是：

- JSON 解析
- 颜色解析
- 字符串构造
- 多次 `displayText()` 调用

这些操作如果在 MQTT 客户端回调中执行，会影响系统实时性，并且在网络抖动、消息频繁到达时容易导致延迟或卡顿。

## 优化目标

- 回调函数尽可能快地返回
- 将耗时的消息处理放到专门的任务中执行
- 保证 MQTT 客户端保持响应性
- 避免回调中执行动态解析与显示逻辑

## 实现思路

### 1. 引入消息队列 / 标志位

在 `WiFiManager` 内部新增一个轻量级缓存结构，用于保存最近一条或多条待处理消息。常见方案：

- 使用 FreeRTOS `QueueHandle_t`
- 使用环形缓冲区
- 使用标志位 + 固定大小消息缓存

回调函数仅将原始 payload 入队，然后立即返回。

### 2. 在任务/主循环中处理队列

新增一个专门负责 MQTT 消息处理的任务，或在 `WiFiManager::loop()` 中加入队列消费逻辑：

- 从队列取出消息
- 解析 JSON
- 执行 `displayManager` 相关显示逻辑
- 在任务上下文中释放缓存内存

这样，耗时逻辑从 MQTT 回调中剥离出来，避免阻塞底层 MQTT 客户端。

### 3. 设计要点

- `mqttCallback()` 仅执行：
  - 复制 topic
  - 复制 payload
  - 将消息发送到 `xQueueSendFromISR()` 或 `xQueueSend()`
- 不在回调中执行 `deserializeJson()` 或 `displayManager.displayText()`
- 专用任务内部执行完整解析和业务逻辑
- 控制队列长度，防止消息爆发时占用太多内存
- 如果消息队列满了，可选择丢弃旧消息、丢弃新消息或设置简单标志位

## 具体实现方案

参考实现可以分为以下几个模块：

### 1. 数据结构

```cpp
struct MqttMessage {
    char topic[64];
    char payload[MAX_MQTT_PAYLOAD_SIZE];
    unsigned int length;
};

static QueueHandle_t mqttMessageQueue;
```

### 2. 初始化队列

```cpp
void WiFiManager::init() {
    mqttMessageQueue = xQueueCreate(4, sizeof(MqttMessage));
    // ... 其他初始化
}
```

### 3. 回调函数改造

```cpp
void WiFiManager::mqttCallback(char* topic, byte* payload, unsigned int length) {
    MqttMessage msg;
    strncpy(msg.topic, topic, sizeof(msg.topic) - 1);
    msg.topic[sizeof(msg.topic) - 1] = '\0';
    msg.length = min(length, (unsigned int)sizeof(msg.payload) - 1);
    memcpy(msg.payload, payload, msg.length);
    msg.payload[msg.length] = '\0';

    xQueueSendFromISR(mqttMessageQueue, &msg, nullptr);
}
```

### 4. 专用处理任务

```cpp
void WiFiManager::mqttTask(void* pvParameters) {
    MqttMessage msg;
    while (true) {
        if (xQueueReceive(mqttMessageQueue, &msg, portMAX_DELAY) == pdTRUE) {
            processMqttMessage(msg.topic, msg.payload, msg.length);
        }
    }
}
```

### 5. 消息处理函数

```cpp
void WiFiManager::processMqttMessage(const char* topic, const char* payload, unsigned int length) {
    String topicStr = String(topic);
    if (topicStr.equals(topicText)) {
        parseAndDisplayText(payload);
    } else if (topicStr.equals(topicClear)) {
        // 解析 JSON 并清屏
    } else if (topicStr.equals(topicBrightness)) {
        // 调整亮度
    } else if (topicStr.equals(topicImage)) {
        parseAndDisplayImage((byte*)payload, length);
    }
}
```

## 进一步优化建议

- 如果系统只需处理一条最新消息，可用单一缓存 + 标志位 `mqttPending = true`，在 `loop()` 里覆盖旧消息。
- 若消息量大，可使用循环队列并设置最大深度。
- 对于 `displayImage()` 这种更耗时操作，可单独使用更大的任务栈或单独任务处理。
- 保证 `xQueueSendFromISR()` 与 `xQueueReceive()` 配合使用，避免在中断或回调中做阻塞等待。

## 总结

这个优化的核心是“回调只做最小工作，耗时处理放到任务上下文”。

- MQTT 回调：快速入队返回
- 专用任务：完整解析、显示、业务处理
- 好处：提高系统实时性，避免 MQTT 客户端阻塞，增强网络不稳定时的稳定性
