# MQTT 消息回调优化说明

## 概述

本系统采用 **FreeRTOS 消息队列 + 独立任务** 的架构来处理 MQTT 回调，实现了异步消息处理，避免阻塞 MQTT 客户端循环，提高系统响应性和稳定性。

---

## 背景

### 原始实现问题

如果直接在 `WiFiManager::mqttCallback()` 中处理 MQTT 消息，会执行以下耗时操作：

- JSON 解析
- 颜色解析和转换
- 字符串构造和内存分配
- 调用 `displayManager.displayText()` 或 `displayManager.displayImage()`
- 图片的 Base64 解码和逐像素绘制

这些操作在 MQTT 回调中执行会导致：

1. **阻塞网络循环**：MQTT 客户端无法及时处理心跳和接收新消息
2. **延迟累积**：网络抖动或消息频繁到达时，延迟会不断累积
3. **连接不稳定**：长时间阻塞可能导致 MQTT 连接超时
4. **看门狗超时**：大图片处理可能触发看门狗重启

---

## 优化目标

- ✅ 回调函数尽可能快地返回（< 1ms）
- ✅ 将耗时的消息处理放到独立任务中执行
- ✅ 保证 MQTT 客户端保持响应性
- ✅ 避免回调中执行动态解析与显示逻辑
- ✅ 支持多个消息排队处理
- ✅ 确保看门狗保护，防止系统重启

---

## 架构设计

### 消息处理流程

```
MQTT消息到达
    ↓
mqttCallback() (快速入队，<1ms)
    ↓
xQueueSend() → 消息队列 (容量8)
    ↓
mqttProcessTask (独立FreeRTOS任务)
    ↓
解析JSON → 执行显示操作 → 释放内存
```

### 核心优势

1. **快速响应**：MQTT 回调只负责入队，立即返回
2. **异步处理**：耗时操作在独立任务中执行
3. **消息缓存**：队列可缓存多个消息，避免消息丢失
4. **看门狗保护**：独立任务可以自主喂狗
5. **负载均衡**：网络核心和显示核心分离，互不干扰

---

## 具体实现

### 1. 数据结构

#### MqttMessageItem 结构体

```cpp
struct MqttMessageItem {
    char topic[64];           // MQTT主题
    byte *payload;            // 消息载荷（动态分配）
    unsigned int length;      // 载荷长度
};
```

**设计要点：**
- topic 使用固定大小数组（64字节），避免动态分配
- payload 使用动态分配，适应不同大小的消息
- 长度字段记录实际数据大小

#### 队列声明

```cpp
// WiFiManager.h
QueueHandle_t mqttMessageQueue;           // MQTT消息队列
TaskHandle_t mqttProcessTaskHandle;       // MQTT消息处理任务句柄
```

---

### 2. 初始化队列和任务

```cpp
void WiFiManager::init() {
    // ... 其他初始化 ...

    // 创建 MQTT 消息处理队列（容量8）
    if (mqttMessageQueue == nullptr) {
        mqttMessageQueue = xQueueCreate(8, sizeof(MqttMessageItem*));
        if (mqttMessageQueue == nullptr) {
            DEBUG_LOG("MQTT消息队列创建失败\n");
        }
    }

    // 创建 MQTT 消息处理任务（固定在Core 0，网络核心）
    if (mqttProcessTaskHandle == nullptr && mqttMessageQueue != nullptr) {
        BaseType_t taskCreated = xTaskCreatePinnedToCore(
            mqttProcessTaskEntry,  // 任务入口函数
            "MqttProc",            // 任务名称
            8192,                   // 任务栈大小（8KB）
            this,                   // 任务参数（WiFiManager指针）
            1,                      // 任务优先级
            &mqttProcessTaskHandle, // 任务句柄
            0                       // 固定在Core 0
        );
        if (taskCreated != pdPASS) {
            DEBUG_LOG("MQTT处理任务创建失败\n");
            mqttProcessTaskHandle = nullptr;
        }
    }
}
```

**配置说明：**
- 队列容量：8个消息项
- 任务栈：8192字节（8KB）
- 任务优先级：1（中等优先级）
- 固定核心：Core 0（网络核心）

---

### 3. 回调函数改造

```cpp
void WiFiManager::mqttCallback(char* topic, byte* payload, unsigned int length) {
    DEBUG_LOG("MQTT回调: 收到消息, 主题=%s, 长度=%d 字节\n", topic, length);

    // 快速入队，立即返回
    if (!enqueueMqttMessage(topic, payload, length)) {
        DEBUG_LOG("MQTT回调：消息入队失败，已丢弃\n");
    } else {
        DEBUG_LOG("MQTT回调：消息已入队\n");
    }
}
```

#### enqueueMqttMessage 实现

```cpp
bool WiFiManager::enqueueMqttMessage(const char* topic, const byte* payload, unsigned int length) {
    if (mqttMessageQueue == nullptr) {
        return false;
    }

    // 分配消息项
    MqttMessageItem *item = (MqttMessageItem *)malloc(sizeof(MqttMessageItem));
    if (item == nullptr) {
        return false;
    }
    memset(item, 0, sizeof(MqttMessageItem));

    // 复制topic
    strncpy(item->topic, topic ? topic : "", sizeof(item->topic) - 1);
    item->topic[sizeof(item->topic) - 1] = '\0';

    // 复制payload
    item->length = length;
    item->payload = (byte *)malloc(length + 1);
    if (item->payload == nullptr) {
        free(item);
        return false;
    }
    if (length > 0) {
        memcpy(item->payload, payload, length);
    }
    item->payload[length] = '\0';

    // 入队
    if (xQueueSend(mqttMessageQueue, &item, 0) != pdTRUE) {
        free(item->payload);
        free(item);
        return false;
    }

    return true;
}
```

**设计要点：**
- 动态分配消息项和payload内存
- 失败时立即释放所有已分配的内存
- 使用非阻塞方式入队（超时0）
- 入队失败时返回false，调用方可以记录日志

---

### 4. 专用处理任务

#### 任务入口函数（静态函数）

```cpp
void WiFiManager::mqttProcessTaskEntry(void *param) {
    WiFiManager *self = static_cast<WiFiManager *>(param);
    if (self) {
        self->mqttProcessTask();
    }
    vTaskDelete(NULL);
}
```

#### 任务主循环

```cpp
void WiFiManager::mqttProcessTask() {
    MqttMessageItem *item = nullptr;

    while (true) {
        // 阻塞等待消息（无限等待）
        if (mqttMessageQueue && xQueueReceive(mqttMessageQueue, &item, portMAX_DELAY) == pdTRUE) {
            if (item == nullptr) {
                continue;
            }

            // 根据主题分发消息
            String topicStr(item->topic);

            if (topicStr.equals(topicText)) {
                parseAndDisplayText((const char *)item->payload);
            } else if (topicStr.equals(topicClear)) {
                // 处理清屏消息
                StaticJsonDocument<128> doc;
                DeserializationError error = deserializeJson(doc, (const char *)item->payload);
                if (error) {
                    DEBUG_LOG("Clear消息JSON解析失败: %s\n", error.c_str());
                } else {
                    bool clear = doc["clear"] | false;
                    if (clear) {
                        displayManager.clearAll();
                        DEBUG_LOG("MQTT回调：已清除显示内容\n");
                    }
                }
            } else if (topicStr.equals(topicBrightness)) {
                // 处理亮度设置消息
                StaticJsonDocument<128> doc;
                DeserializationError error = deserializeJson(doc, (const char *)item->payload);
                if (error) {
                    DEBUG_LOG("Brightness消息JSON解析失败: %s\n", error.c_str());
                } else {
                    int b = doc["brightness"] | 128;
                    if (b < 0) b = 0;
                    if (b > 255) b = 255;
                    displayManager.setBrightness((uint8_t)b);
                    DEBUG_LOG("MQTT回调：已设置亮度 %d\n", b);
                }
            } else if (topicStr.equals(topicImage)) {
                // 处理图片消息
                parseAndDisplayImage(item->payload, item->length);
            } else {
                DEBUG_LOG("未知主题: %s\n", item->topic);
            }

            // 释放内存
            if (item->payload) {
                free(item->payload);
            }
            free(item);
        }
    }
}
```

**设计要点：**
- 使用 `portMAX_DELAY` 阻塞等待，节省CPU
- 队列中存储的是指针（MqttMessageItem*），避免数据拷贝
- 处理完成后立即释放内存，避免内存泄漏
- 每个消息处理是原子性的，不会被中断

---

### 5. 图片处理看门狗保护

```cpp
void DisplayManager::displayImage(const char *base64Data, int length) {
    // ... Base64解码 ...

    // 将rgb565数据绘制到屏幕
    for (int i = 0; i < pixelCount; i++) {
        // ... 绘制逻辑 ...

        // 每256个像素喂狗一次，避免看门狗超时
        if (i % 256 == 0) {
            esp_task_wdt_reset();
        }

        // 每1024个像素输出进度（避免过于频繁）
        if (i % 1024 == 0) {
            DEBUG_LOG("displayImage: 绘制进度 %d/%d (%.1f%%), 耗时: %lu ms\n",
                     i, pixelCount, (i * 100.0) / pixelCount, millis() - startTime);
        }
    }

    // ...
}
```

---

## 设计要点总结

### 回调函数职责

`mqttCallback()` **仅执行**：
1. 复制 topic 到固定大小数组
2. 动态分配 payload 内存
3. 将消息指针发送到队列
4. 立即返回（通常 < 1ms）

### 回调函数**不执行**

- ❌ 不执行 `deserializeJson()`
- ❌ 不执行 `displayManager.displayText()`
- ❌ 不执行 `displayManager.displayImage()`
- ❌ 不执行任何耗时操作

### 专用任务职责

`mqttProcessTask()` **执行完整**：
1. 从队列取出消息
2. 解析 JSON
3. 执行业务逻辑
4. 释放内存
5. 循环等待下一条消息

### 内存管理

- **分配**：在回调中动态分配消息项和payload
- **传递**：通过队列传递消息项指针
- **释放**：在任务中释放所有动态分配的内存
- **安全**：入队失败时立即释放已分配的内存

### 队列管理

- **容量**：8个消息项（可根据需要调整）
- **阻塞策略**：回调中非阻塞（超时0），任务中阻塞（无限等待）
- **溢出处理**：队列满时丢弃新消息，返回false
- **内存安全**：队列存储指针，不拷贝数据

---

## 性能分析

### 回调函数性能

| 操作 | 耗时 |
|------|------|
| 复制topic | < 0.1ms |
| 分配内存 | < 0.5ms |
| 入队操作 | < 0.1ms |
| **总计** | **< 1ms** |

### 任务处理性能

| 消息类型 | 处理耗时 |
|----------|----------|
| 文本消息 | 5-20ms |
| 清屏消息 | < 5ms |
| 亮度消息 | < 5ms |
| 图片消息（64x64）| 100-500ms |

### 系统响应性

- **MQTT心跳**：不受影响，持续发送
- **消息接收**：不被阻塞，可以连续接收
- **看门狗**：图片处理中有保护，不会超时

---

## 调试日志

### 回调函数日志

```
MQTT回调: 收到消息, 主题=LED/246f28a58ec4/Text, 长度=45 字节
MQTT回调：消息已入队
```

### 任务处理日志

```
MQTT消息处理: 收到文本消息, payload长度=45 字节
parseAndDisplayText: 开始处理, 时间=12345 ms
parseAndDisplayText: JSON解析完成, 耗时=2 ms
parseAndDisplayText: 文本='Hello', 滚动=true, 字号=1, 行号=1, ...
parseAndDisplayText: 颜色解析完成, RGB565=65535, 耗时=3 ms
displayText: 调用displayText...
displayText: 滚动文本已保存, 总耗时=8 ms
parseAndDisplayText: displayText返回, 总耗时=12 ms
MQTT消息处理: 文本消息处理完成
```

### 图片处理日志

```
MQTT消息处理: 收到图片消息, 大小=5461 字节
parseAndDisplayImage: 开始处理, 时间=12345 ms, 大小=5461 字节
parseAndDisplayImage: 提取base64数据, 长度=5461 字节, 耗时=2 ms
parseAndDisplayImage: 调用displayImage...
displayImage: 开始处理图片, 时间=12347 ms
displayImage: base64输入 5461 字节, 预计解码 4096 字节
displayImage: 内存分配成功, 耗时=1 ms
displayImage: 解码完成, 耗时=5 ms
displayImage: 开始绘制像素...
displayImage: 绘制进度 1024/2048 (50.0%), 耗时=150 ms
displayImage: 绘制进度 2048/2048 (100.0%), 耗时=280 ms
displayImage: 像素绘制完成, 耗时=280 ms
displayImage: 缓冲区翻转完成, 总耗时=285 ms
displayImage: 完成! 总共绘制 2048 像素, 总耗时 290 ms
parseAndDisplayImage: displayImage返回, 总耗时=295 ms
MQTT回调：图片处理完成
```

---

## 进一步优化建议

### 1. 队列容量调整

根据实际消息负载调整队列容量：

```cpp
// 低负载场景
mqttMessageQueue = xQueueCreate(4, sizeof(MqttMessageItem*));

// 高负载场景
mqttMessageQueue = xQueueCreate(16, sizeof(MqttMessageItem*));
```

### 2. 优先级队列

实现优先级队列，图片消息优先处理：

```cpp
// 定义消息优先级
enum MessagePriority {
    PRIORITY_LOW = 0,      // 文本消息
    PRIORITY_MEDIUM = 1,   // 亮度/清屏
    PRIORITY_HIGH = 2      // 图片消息
};

// 为不同优先级创建不同队列
QueueHandle_t lowPriorityQueue;
QueueHandle_t mediumPriorityQueue;
QueueHandle_t highPriorityQueue;
```

### 3. 消息去重

对重复消息进行去重：

```cpp
// 在入队前检查是否已有相同主题的消息在队列中
if (isDuplicateMessageInQueue(topic)) {
    free(item->payload);
    free(item);
    return false;
}
```

### 4. 批量处理

对多个小消息进行批量处理：

```cpp
// 每次从队列取出多条消息
for (int i = 0; i < BATCH_SIZE && xQueueReceive(..., 0) == pdTRUE; i++) {
    processMessage(items[i]);
}
```

### 5. 内存池

使用内存池避免频繁的 malloc/free：

```cpp
// 预分配消息池
#define MESSAGE_POOL_SIZE 16
static MqttMessageItem messagePool[MESSAGE_POOL_SIZE];
static bool messagePoolUsed[MESSAGE_POOL_SIZE];
```

---

## 常见问题

### Q1：为什么队列满时丢弃新消息？

丢弃新消息可以保证旧消息被处理，避免消息积压导致系统崩溃。

### Q2：队列容量设置多少合适？

建议设置为 8-16 个消息项。过小会丢失消息，过大会占用过多内存。

### Q3：图片处理时间长会影响后续消息吗？

不会。图片在独立任务中处理，不会阻塞回调，但后续消息会在队列中等待。

### Q4：如何查看消息处理状态？

查看串口日志，每条消息都有详细的时间戳和耗时记录。

### Q5：看门狗超时时间是多少？

30秒。图片处理中有喂狗保护，不会超时。

---

## 总结

本系统的 MQTT 消息处理优化采用 **"回调快速入队，任务异步处理"** 的设计模式，实现了：

1. **快速响应**：MQTT 回调 < 1ms 返回
2. **异步处理**：耗时操作在独立任务中执行
3. **消息缓存**：队列可缓存多个消息
4. **看门狗保护**：大图片处理时自主喂狗
5. **内存安全**：及时释放动态分配的内存
6. **调试友好**：完整的日志输出

这种设计大大提高了系统的实时性和稳定性，适合在生产环境中运行。
