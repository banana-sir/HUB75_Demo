# MQTT客户端使用文档

## 目录
1. [概述](#概述)
2. [系统架构](#系统架构)
3. [MQTT连接信息](#mqtt连接信息)
4. [设备识别](#设备识别)
5. [主题列表](#主题列表)
   - [LED/{mac}/Text](#ledmactext-文本显示)
   - [LED/{mac}/Clear](#ledmacclear-清屏)
   - [LED/{mac}/Brightness](#ledmacbrightness-亮度设置)
   - [LED/{mac}/Image](#ledmacimage-图片显示)
6. [设备分辨率适配](#设备分辨率适配)
7. [消息限制](#消息限制)
8. [完整示例](#完整示例)
9. [常见问题](#常见问题)

---

## 概述

本系统通过MQTT协议控制ESP32 LED矩阵显示屏，支持多设备独立管理和不同分辨率的自动适配。每个设备根据其MAC地址订阅独立的MQTT主题，实现设备级别的消息隔离。

### 核心特性
- ✅ 每个设备独立的MQTT主题，天然隔离
- ✅ 无需target字段，简化消息格式
- ✅ 根据设备分辨率自动适配行数限制
- ✅ JSON格式消息，易于解析
- ✅ 支持文本、图片、亮度等多种控制方式
- ✅ 异步消息处理，避免阻塞网络循环
- ✅ 完善的调试日志系统

---

## 系统架构

### 双核心FreeRTOS任务架构

```
┌─────────────────────────────────────────────────────┐
│                ESP32 Dual Core                 │
├─────────────────────────────────────────────────────┤
│  Core 1 (应用核心)    │  Core 0 (网络核心)  │
│  ┌──────────────┐    │  ┌──────────────┐     │
│  │DisplayManager │    │  │ WiFiManager  │     │
│  │              │    │  │              │     │
│  │ • LED驱动    │    │  │ • WiFi连接   │     │
│  │ • 文本渲染  │    │  │ • MQTT通信   │     │
│  │ • 图片显示  │    │  │ • AP配网     │     │
│  │ • 滚动动画  │    │  │ • 消息队列   │     │
│  └──────────────┘    │  └──────────────┘     │
│  ┌──────────────┐    │  ┌──────────────┐     │
│  │ DisplayTask  │    │  │  MqttProc   │     │
│  │  (高优先级) │    │  │  (独立任务)  │     │
│  └──────────────┘    │  └──────────────┘     │
│  50fps 渲染         │  异步消息处理      │
└─────────────────────────────────────────────────────┘
```

### 消息处理流程

```
MQTT消息到达
    ↓
mqttCallback() (快速入队)
    ↓
xQueueSend() → 消息队列
    ↓
mqttProcessTask (独立任务)
    ↓
解析JSON → 执行显示操作
```

**优势：**
- MQTT回调快速返回，不阻塞网络循环
- 独立任务处理耗时操作
- 避免消息积压导致延迟

---

## MQTT连接信息

### 服务器配置

| 配置项 | 值 |
|--------|-----|
| 服务器地址 | zheng221.xyz |
| 端口 | 1883 |
| 用户名 | 64x32esp32（根据分辨率自动选择）|
| 密码 | 123456 |

### 设备ClientId格式

```
ESP32/{分辨率}/{MAC地址}
```

**示例：**
- `ESP32/64x64/246f28a58ec4`
- `ESP32/64x32/aabbccdd1122`
- `ESP32/32x32/ccddeeff3344`

### 设备主题格式

每个设备根据MAC地址订阅独立的主题：

```
LED/{MAC地址}/{功能}
```

**示例：**
- `LED/246f28a58ec4/Text` - 文本显示
- `LED/246f28a58ec4/Clear` - 清屏
- `LED/246f28a58ec4/Brightness` - 亮度设置
- `LED/246f28a58ec4/Image` - 图片显示

### 最大消息大小

- **MQTT消息最大载荷**: 12800 字节
- 超过此大小的消息将被拒绝
- 64x64屏幕的Base64图片约10923字节（在限制内）

---

## 设备识别

### MAC地址获取方式

设备的MAC地址通过以下方式获取：

**方式1：从设备串口输出获取**
```
MAC: 246f28a58ec4
MQTT ClientId: ESP32/64x32/246f28a58ec4
MQTT Topics:
  Text: LED/246f28a58ec4/Text
  Clear: LED/246f28a58ec4/Clear
  Brightness: LED/246f28a58ec4/Brightness
  Image: LED/246f28a58ec4/Image
```

**方式2：通过设备信息接口获取**（如果有后端管理系统）

**方式3：使用MQTT订阅列表工具查看**
- 连接到MQTT服务器
- 查看所有订阅的主题
- 根据ClientId格式 `ESP32/{分辨率}/{MAC地址}` 解析

### 设备信息示例

从串口输出中可以获取完整设备信息：
```
MAC: 246f28a58ec4
MQTT ClientId: ESP32/64x32/246f28a58ec4
MQTT Topics:
  Text: LED/246f28a58ec4/Text
  Clear: LED/246f28a58ec4/Clear
  Brightness: LED/246f28a58ec4/Brightness
  Image: LED/246f28a58ec4/Image
```

---

## 主题列表

### LED/{mac}/Text - 文本显示

#### 消息格式

```json
{
  "text": "要显示的文本",
  "scroll_mode": false,
  "font_size": 1,
  "color": "#FFFFFF",
  "line": 1,
  "wrap": true,
  "scroll_speed": 2,
  "scroll_direction": 0
}
```

#### 字段说明

| 字段 | 类型 | 必需 | 默认值 | 说明 |
|------|------|------|--------|------|
| text | string | 是 | - | 要显示的文本内容（支持UTF-8中文）|
| scroll_mode | boolean | 否 | false | 是否滚动模式 |
| font_size | int | 否 | 1 | 字体大小（1=小，2=大）|
| color | string | 否 | "#FFFFFF" | 文本颜色（十六进制RGB）|
| line | int | 否 | 1 | 显示行号（见下方详解）|
| wrap | boolean | 否 | true | 静态文本自动换行；滚动模式下无效 |
| scroll_speed | int | 否 | 2 | 滚动速度（1=慢，2=中，3=快）|
| scroll_direction | int | 否 | 0 | 滚动方向（0=向左，1=向右）|

#### 行号限制（根据分辨率）

| 分辨率 | 最大行数 | 行号范围 |
|--------|----------|----------|
| 64x32 | 2 | 1-2 |
| 64x64 | 4 | 1-4 |
| 64x128 | 8 | 1-8 |

**计算公式**: `最大行数 = PANEL_RES_Y / 16`

#### 注意事项

⚠️ **重要**: `line`字段必须根据目标设备的分辨率设置

1. **滚动模式** (`scroll_mode = true`):
   - `line` 必须在 `1` 到 `最大行数` 范围内
   - 不同字体大小的文本可以同时滚动
   - 滚动文本不能全屏显示
   - `wrap` 参数无效，滚动模式下会被忽略

2. **静态模式** (`scroll_mode = false`):
   - `line <= 0`（包括-1）时：清屏并全屏显示
   - `line > 0` 时：在指定行显示，保留屏幕其他区域
   - `wrap` 仅对静态文本有效
   - `autoWrap = true` 时，系统会自动计算文本占用行数并换行

3. **字体大小与覆盖**:
   - `font_size = 1`: 高度16像素，占1行
   - `font_size = 2`: 高度32像素，占2行

#### 使用示例

**示例1：向MAC为246f28a58ec4的设备发送滚动文本**
```json
{
  "text": "欢迎使用LED显示屏",
  "scroll_mode": true,
  "font_size": 1,
  "color": "#00FF00",
  "line": 1,
  "scroll_speed": 2,
  "scroll_direction": 0
}
```
发布到主题：`LED/246f28a58ec4/Text`

**示例2：向MAC为aabbccdd1122的设备发送静态文本**
```json
{
  "text": "Hello World",
  "scroll_mode": false,
  "font_size": 2,
  "color": "#FF0000",
  "line": 1,
  "wrap": false
}
```
发布到主题：`LED/aabbccdd1122/Text`

**示例3：多行滚动文本（64x64设备）**

第1行：
```json
{
  "text": "第1行 - 小字体",
  "scroll_mode": true,
  "font_size": 1,
  "line": 1,
  "scroll_speed": 1
}
```

第2行：
```json
{
  "text": "第2行 - 大字体",
  "scroll_mode": true,
  "font_size": 2,
  "line": 2,
  "scroll_speed": 2
}
```

---

### LED/{mac}/Clear - 清屏

#### 消息格式

```json
{
  "clear": true
}
```

#### 字段说明

| 字段 | 类型 | 必需 | 默认值 | 说明 |
|------|------|------|--------|------|
| clear | boolean | 是 | true | 是否清屏 |

#### 注意事项

- `clear` 必须为 `true` 才会执行清屏操作
- 清屏会停止所有滚动文本并释放相关内存
- 清屏会退出图片显示模式
- 无需考虑设备分辨率限制

#### 使用示例

**示例1：清空MAC为246f28a58ec4的设备屏幕**
```json
{
  "clear": true
}
```
发布到主题：`LED/246f28a58ec4/Clear`

---

### LED/{mac}/Brightness - 亮度设置

#### 消息格式

```json
{
  "brightness": 128
}
```

#### 字段说明

| 字段 | 类型 | 必需 | 默认值 | 说明 |
|------|------|------|--------|------|
| brightness | int | 是 | 128 | 亮度值（0-255）|

#### 注意事项

- `brightness` 范围：0（最暗）到 255（最亮）
- 超出范围会自动限制在 0-255 之间
- 无需考虑设备分辨率限制
- 默认亮度为 128（50%）

#### 使用示例

**示例1：设置MAC为246f28a58ec4的设备亮度为128（中等亮度）**
```json
{
  "brightness": 128
}
```
发布到主题：`LED/246f28a58ec4/Brightness`

**示例2：设置MAC为aabbccdd1122的设备为最亮**
```json
{
  "brightness": 255
}
```
发布到主题：`LED/aabbccdd1122/Brightness`

---

### LED/{mac}/Image - 图片显示

#### 消息格式

```json
{
  "image_base64": "base64编码的RGB565图像数据"
}
```

#### 字段说明

| 字段 | 类型 | 必需 | 默认值 | 说明 |
|------|------|------|--------|------|
| image_base64 | string | 是 | - | Base64编码的RGB565图像数据 |

#### 图像要求

| 项目 | 要求 |
|------|------|
| 颜色格式 | RGB565（每个像素2字节，小端序）|
| 图像尺寸 | 必须匹配目标设备的分辨率（PANEL_RES_X × PANEL_RES_Y）|
| 编码方式 | Base64编码 |
| 数据大小 | 不得超过MAX_MQTT_PAYLOAD_SIZE（12800字节）|

#### Base64编码大小计算

对于64x32分辨率的屏幕：
- 原始大小：64 × 32 × 2 = 4096 字节
- Base64编码后大小：4096 × 4/3 ≈ 5461 字节（在限制范围内）

对于64x64分辨率的屏幕：
- 原始大小：64 × 64 × 2 = 8192 字节
- Base64编码后大小：8192 × 4/3 ≈ 10923 字节（在限制范围内）

#### 注意事项

⚠️ **重要**: 图片分辨率必须与目标设备分辨率完全匹配

- 64x32的图片只能发送给64x32的设备
- 64x64的图片只能发送给64x64的设备
- 不匹配的图片会导致显示异常
- 显示图片会自动退出图片模式，清除所有文本内容
- 显示图片后会进入图片显示模式，loop()会跳过渲染以保持图片静止
- 调用 displayText() 或 clearAll() 会退出图片模式，恢复文本渲染

#### 使用示例

**示例1：向MAC为246f28a58ec4的设备发送图片**
```json
{
  "image_base64": "AQIDBAUGBwgJCgsMDQ4PEBESExQVFhcYGRobHB0eHyAhIiMkJSYnKCkqKywtLi8wMTIzNDU2Nzg5Ojs8PT4/QEFCQ0RFRkdISUpLTE1OT1BRUlNUVVZXWFlaW1xdXl9gYWJjZGVmZ2hpamtsbW5vcHFyc3R1dnd4eXp7fH1+f4CBgoOEhYaHiImKi4yNjo+QkZKTlJWWl5iZmpucnZ6foKGio6SlpqeoqaqrrK2ur7CxsrO0tba3uLm6u7y9vr/AwcLDxMXGx8jJysvMzc7P0NHS09TV1tfY2drb3N3e3+Dh4uPk5ebn6Onq6+zt7u/w8fLz9PX29/j5+vv8/f7/"
}
```
发布到主题：`LED/246f28a58ec4/Image`

---

## 设备分辨率适配

### 支持的分辨率

| 分辨率 | 最大行数 | 说明 |
|--------|----------|------|
| 64x32 | 2 | 32像素高 / 16像素每行 = 2行 |
| 64x64 | 4 | 64像素高 / 16像素每行 = 4行 |
| 64x128 | 8 | 128像素高 / 16像素每行 = 8行 |

### 如何判断设备分辨率

通过解析设备的`clientId`：

```
ESP32/{分辨率}/{MAC地址}
```

**解析示例：**
- `ESP32/64x64/246f28a58ec4` → 分辨率为 64x64
- `ESP32/64x32/aabbccdd1122` → 分辨率为 64x32

### 分辨率适配表

| 分辨率 | PANEL_RES_X | PANEL_RES_Y | maxLines | line有效范围 |
|--------|------------|-------------|----------|--------------|
| 64x32 | 64 | 32 | 2 | 1-2 |
| 64x64 | 64 | 64 | 4 | 1-4 |
| 64x128 | 64 | 128 | 8 | 1-8 |

### Python代码示例：自动适配分辨率

```python
def get_max_lines(resolution):
    """
    根据设备分辨率获取最大行数
    """
    width, height = map(int, resolution.split('x'))
    return height // 16

# 示例
resolution = "64x64"
max_lines = get_max_lines(resolution)
print(f"分辨率 {resolution} 的最大行数: {max_lines}")
# 输出: 分辨率 64x64 的最大行数: 4
```

### 行号限制检查表

| 分辨率 | line=1 | line=2 | line=3 | line=4 | line=5+ |
|--------|--------|--------|--------|--------|--------|
| 64x32 | ✅ | ✅ | ❌ | ❌ | ❌ |
| 64x64 | ✅ | ✅ | ✅ | ✅ | ❌ |
| 64x128 | ✅ | ✅ | ✅ | ✅ | ✅ |

---

## 消息限制

### 通用限制

| 限制项 | 值 | 说明 |
|--------|---|------|
| 最大消息大小 | 12800 字节 | 超过此大小的消息将被拒绝 |

### 各主题具体限制

#### LED/{mac}/Text

| 限制项 | 值 |
|--------|-----|
| JSON文档大小 | ≤ 512 字节 |
| 文本长度 | 无硬性限制（受JSON大小限制）|
| 行号范围 | 1 到 `PANEL_RES_Y / 16` |
| 字体大小 | 1-2 |
| 滚动速度 | 1-3 |

#### LED/{mac}/Clear

| 限制项 | 值 |
|--------|-----|
| JSON文档大小 | ≤ 64 字节 |

#### LED/{mac}/Brightness

| 限制项 | 值 |
|--------|-----|
| JSON文档大小 | ≤ 64 字节 |
| 亮度值范围 | 0-255 |

#### LED/{mac}/Image

| 限制项 | 值 |
|--------|-----|
| Base64数据大小 | ≤ 12800 字节 |
| 图像分辨率 | 必须匹配目标设备分辨率 |

### 超过限制的处理

- 超过大小限制的消息会被设备拒绝
- 会在串口打印警告信息
- 不会影响设备正常工作

---

## 完整示例

### Python示例

```python
import paho.mqtt.client as mqtt
import json

# MQTT连接配置
MQTT_SERVER = "zheng221.xyz"
MQTT_PORT = 1883
MQTT_USERNAME = "64x32esp32"
MQTT_PASSWORD = "123456"

# 设备MAC地址（从串口输出获取）
DEVICE_MAC = "246f28a58ec4"

# 生成设备特定的主题
TOPIC_TEXT = f"LED/{DEVICE_MAC}/Text"
TOPIC_CLEAR = f"LED/{DEVICE_MAC}/Clear"
TOPIC_BRIGHTNESS = f"LED/{DEVICE_MAC}/Brightness"
TOPIC_IMAGE = f"LED/{DEVICE_MAC}/Image"

# 创建MQTT客户端
client = mqtt.Client()

# 设置用户名和密码
client.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD)

# 连接到MQTT服务器
client.connect(MQTT_SERVER, MQTT_PORT, 60)

# ========================================
# 示例1：发送滚动文本
# ========================================
text_msg = {
    "text": "欢迎使用LED显示屏",
    "scroll_mode": True,
    "font_size": 1,
    "color": "#FFFFFF",
    "line": 1,
    "scroll_speed": 2,
    "scroll_direction": 0
}
client.publish(TOPIC_TEXT, json.dumps(text_msg))

# ========================================
# 示例2：发送多行滚动文本
# ========================================
# 第1行
text_msg1 = {
    "text": "第1行 - 小字体",
    "scroll_mode": True,
    "font_size": 1,
    "line": 1,
    "scroll_speed": 1
}
client.publish(TOPIC_TEXT, json.dumps(text_msg1))

# 第2行（大字体）
text_msg2 = {
    "text": "第2行 - 大字体",
    "scroll_mode": True,
    "font_size": 2,
    "line": 2,
    "scroll_speed": 2
}
client.publish(TOPIC_TEXT, json.dumps(text_msg2))

# ========================================
# 示例3：清空屏幕
# ========================================
clear_msg = {
    "clear": True
}
client.publish(TOPIC_CLEAR, json.dumps(clear_msg))

# ========================================
# 示例4：设置亮度
# ========================================
brightness_msg = {
    "brightness": 128
}
client.publish(TOPIC_BRIGHTNESS, json.dumps(brightness_msg))

# ========================================
# 示例5：发送图片
# ========================================
image_msg = {
    "image_base64": "AQIDBAUGBwgJCgsMDQ4PEBESExQVFhcYGRobHB0eHyAhIiMkJSYnKCkqKywtLi8wMTIzNDU2Nzg5Ojs8PT4/QEFCQ0RFRkdISUpLTE1OT1BRUlNUVVZXWFlaW1xdXl9gYWJjZGVmZ2hpamtsbW5vcHFyc3R1dnd4eXp7fH1+f4CBgoOEhYaHiImKi4yNjo+QkZKTlJWWl5iZmpucnZ6foKGio6SlpqeoqaqrrK2ur7CxsrO0tba3uLm6u7y9vr/AwcLDxMXGx8jJysvMzc7P0NHS09TV1tfY2drb3N3e3+Dh4uPk5ebn6Onq6+zt7u/w8fLz9PX29/j5+vv8/f7/"
}
client.publish(TOPIC_IMAGE, json.dumps(image_msg))

# 断开连接
client.disconnect()
```

### JavaScript/Node.js示例

```javascript
const mqtt = require('mqtt');

// MQTT连接配置
const MQTT_SERVER = "zheng221.xyz";
const MQTT_PORT = 1883;
const MQTT_USERNAME = "64x32esp32";
const MQTT_PASSWORD = "123456";

// 设备MAC地址
const DEVICE_MAC = "246f28a58ec4";

// 生成设备特定的主题
const TOPIC_TEXT = `LED/${DEVICE_MAC}/Text`;
const TOPIC_CLEAR = `LED/${DEVICE_MAC}/Clear`;
const TOPIC_BRIGHTNESS = `LED/${DEVICE_MAC}/Brightness`;
const TOPIC_IMAGE = `LED/${DEVICE_MAC}/Image`;

// 连接到MQTT服务器
const client = mqtt.connect(`mqtt://${MQTT_SERVER}:${MQTT_PORT}`, {
    username: MQTT_USERNAME,
    password: MQTT_PASSWORD
});

client.on('connect', () => {
    console.log('已连接到MQTT服务器');

    // 示例1：发送滚动文本
    const textMsg = {
        text: "欢迎使用LED显示屏",
        scroll_mode: true,
        font_size: 1,
        color: "#FFFFFF",
        line: 1,
        scroll_speed: 2,
        scroll_direction: 0
    };
    client.publish(TOPIC_TEXT, JSON.stringify(textMsg));

    // 示例2：清空屏幕
    const clearMsg = {
        clear: true
    };
    client.publish(TOPIC_CLEAR, JSON.stringify(clearMsg));

    // 示例3：设置亮度
    const brightnessMsg = {
        brightness: 128
    };
    client.publish(TOPIC_BRIGHTNESS, JSON.stringify(brightnessMsg));

    // 示例4：发送图片
    const imageMsg = {
        image_base64: "AQIDBAUGBwgJCgsMDQ4PEBESExQVFhcYGRobHB0eHyAhIiMkJSYnKCkqKywtLi8wMTIzNDU2Nzg5Ojs8PT4/QEFCQ0RFRkdISUpLTE1OT1BRUlNUVVZXWFlaW1xdXl9gYWJjZGVmZ2hpamtsbW5vcHFyc3R1dnd4eXp7fH1+f4CBgoOEhYaHiImKi4yNjo+QkZKTlJWWl5iZmpucnZ6foKGio6SlpqeoqaqrrK2ur7CxsrO0tba3uLm6u7y9vr/AwcLDxMXGx8jJysvMzc7P0NHS09TV1tfY2drb3N3e3+Dh4uPk5ebn6Onq6+zt7u/w8fLz9PX29/j5+vv8/f7/"
    };
    client.publish(TOPIC_IMAGE, JSON.stringify(imageMsg));
});

client.on('error', (error) => {
    console.error('MQTT错误:', error);
});
```

---

## 常见问题

### Q1：如何获取设备的MAC地址？

设备的MAC地址可以从设备串口输出中获取：
```
MAC: 246f28a58ec4
MQTT Topics:
  Text: LED/246f28a58ec4/Text
  ...
```
可以从clientid中解析获取

### Q2：如何同时控制多个设备？

每个设备有独立的主题，需要为每个设备发送单独的消息：

```python
# 控制设备1
client.publish("LED/246f28a58ec4/Text", json.dumps(msg1))

# 控制设备2
client.publish("LED/aabbccdd1122/Text", json.dumps(msg2))
```

### Q3：不同分辨率的设备如何管理？

建议：
1. 为每个设备建立独立的配置记录（MAC地址 + 分辨率）
2. 根据设备分辨率限制line等参数的范围
3. 为不同分辨率的设备准备不同尺寸的图片

### Q4：为什么64x32设备的line=3无效？

64x32分辨率的屏幕最大行数为2（`32 / 16 = 2`），所以line的有效范围是1-2。line=3会超出限制。

### Q5：图片消息的大小限制是多少？

Base64编码后的图片数据不得超过12800字节。对于64x64分辨率的屏幕，Base64编码后约为10923字节，在限制范围内。

### Q6：如何发送不同字体大小的滚动文本？

只需在发送消息时设置不同的`font_size`即可。不同字体大小的文本可以同时滚动。

```python
# 第1行：小字体
text_msg1 = {
    "text": "小字体",
    "scroll_mode": True,
    "font_size": 1,
    "line": 1
}

# 第2行：大字体
text_msg2 = {
    "text": "大字体",
    "scroll_mode": True,
    "font_size": 2,
    "line": 2
}
```

### Q7：静态文本的wrap参数有什么作用？

- `wrap = true`: 文本自动换行，适合长文本
- `wrap = false`: 文本不自动换行，超出部分不显示

**注意**：`wrap` 仅对静态文本有效，滚动模式下该参数会被忽略。

### Q8：如何停止所有滚动文本？

发送清屏消息：
```json
{
  "clear": true
}
```
发布到对应设备的`LED/{mac}/Clear`主题。

### Q9：消息发送失败怎么办？

1. 检查MQTT服务器连接是否正常
2. 检查MAC地址是否正确
3. 检查消息大小是否超过限制
4. 检查JSON格式是否正确
5. 查看设备串口输出获取错误信息

### Q10：如何批量控制多个设备？

创建一个设备列表，循环发送消息：

```python
devices = [
    {"mac": "246f28a58ec4", "resolution": "64x64"},
    {"mac": "aabbccdd1122", "resolution": "64x32"},
    {"mac": "ccddeeff3344", "resolution": "64x64"}
]

for device in devices:
    topic = f"LED/{device['mac']}/Text"
    msg = {
        "text": "欢迎使用",
        "scroll_mode": True,
        "font_size": 1,
        "line": 1
    }
    client.publish(topic, json.dumps(msg))
```

### Q11：图片显示后为什么不显示文本？

正常现象。进入图片显示模式后，loop()会跳过渲染以保持图片静止。调用displayText()会自动退出图片模式，恢复文本渲染。

### Q12：MQTT消息处理速度慢怎么办？

系统已采用异步消息处理机制，MQTT回调会快速入队返回，独立任务处理耗时操作。如果仍然感觉慢：

1. 检查设备串口输出，查看处理耗时
2. 检查网络连接质量
3. 减小消息频率

---

## 附录

### 颜色格式参考

| 颜色 | 十六进制值 |
|------|-----------|
| 黑色 | #000000 |
| 白色 | #FFFFFF |
| 红色 | #FF0000 |
| 绿色 | #00FF00 |
| 蓝色 | #0000FF |
| 黄色 | #FFFF00 |
| 粉色 | #FF00FF |
| 青色 | #00FFFF |

### 滚动速度参考

| 速度等级 | 说明 | 帧间隔（毫秒）|
|---------|------|----------------|
| 1 | 慢速 | 45ms |
| 2 | 中速 | 35ms |
| 3 | 快速 | 20ms |

### 字体大小参考

| 字体大小 | 高度（像素）| 说明 |
|---------|------------|------|
| 1 | 16 | 小字体 |
| 2 | 32 | 大字体 |

---

## 总结

使用MQTT客户端控制LED显示屏的关键点：

1. **设备主题**: 每个设备根据MAC地址有独立的主题：`LED/{mac}/{功能}`
2. **MAC地址**: 从设备串口输出获取，用于构建主题
3. **分辨率适配**: 根据设备分辨率限制line等参数的范围
4. **消息大小**: 注意消息大小限制，避免超过12800字节
5. **错误处理**: 查看设备串口输出获取详细的错误信息
6. **批量控制**: 使用设备列表循环发送消息
7. **异步处理**: 系统采用异步消息处理，保证网络循环的响应性
8. **图片模式**: 图片显示后会进入图片模式，调用displayText会自动退出

遵循本文档的说明，可以正确、高效地控制LED矩阵显示屏。
