# MQTT客户端使用文档

## 目录
1. [概述](#概述)
2. [MQTT连接信息](#mqtt连接信息)
3. [target字段详解](#target字段详解)
4. [主题列表](#主题列表)
   - [LED/Text](#ledtext-文本显示)
   - [LED/Clear](#ledclear-清屏)
   - [LED/Brightness](#ledbrightness-亮度设置)
   - [LED/Image](#ledimage-图片显示)
5. [设备分辨率适配](#设备分辨率适配)
6. [消息限制](#消息限制)
7. [完整示例](#完整示例)
8. [常见问题](#常见问题)

---

## 概述

本系统通过MQTT协议控制LED矩阵显示屏，支持多设备管理和不同分辨率的适配。所有消息都包含`target`字段，用于指定消息的目标设备。

### 核心特性
- ✅ 支持多设备独立控制
- ✅ 根据设备分辨率自动适配行数限制
- ✅ 通配符支持同一分辨率的所有设备
- ✅ JSON格式消息，易于解析
- ✅ 支持文本、图片、亮度等多种控制方式

---

## MQTT连接信息

### 服务器配置

| 配置项 | 值 |
|--------|-----|
| 服务器地址 | zheng241.xyz |
| 端口 | 1883 |
| 用户名 | 64x64esp32 |
| 密码 | 123456 |

### 设备ClientId格式

```
ESP32/{分辨率}/{MAC地址}
```

**示例：**
- `ESP32/64x64/aabbccdd1122`
- `ESP32/64x32/aabbccdd1122`
- `ESP32/32x32/aabbccdd1122`

### 最大消息大小

- **MQTT消息最大载荷**: 12800 字节
- 超过此大小的消息将被拒绝

---

## target字段详解

### target字段的作用

`target`字段用于指定消息的目标设备，确保消息只被目标设备接收和处理。

### target字段的格式

| 格式 | 说明 | 示例 |
|------|------|------|
| 精确匹配 | 指定某个具体设备 | `"target": "ESP32/64x64/aabbccdd1122"` |
| 通配符 | 指定所有相同分辨率的设备 | `"target": "All_64x64"` |

### 通配符格式

```
All_{宽度}x{高度}
```

**示例：**
- `"All_64x64"` - 所有64x64分辨率的设备
- `"All_64x32"` - 所有64x32分辨率的设备
- `"All_32x32"` - 所有32x32分辨率的设备

### target匹配规则

1. **优先级**: 精确匹配 > 通配符匹配
2. **不匹配**: 如果target与当前设备的ClientId或通配符都不匹配，消息将被忽略
3. **必需字段**: 所有消息都必须包含`target`字段

### 代码实现

```cpp
// 检查目标是否匹配当前设备或通配符
String allDevicesTarget = "All_" + String(PANEL_RES_X) + "x" + String(PANEL_RES_Y);

if (!targetStr.equals(clientId) && !targetStr.equals(allDevicesTarget)) {
    Serial.printf("目标不匹配: 收到=%s, 当前ClientId=%s, 通配符=%s\n",
                  targetStr.c_str(), clientId.c_str(), allDevicesTarget.c_str());
    return false;
}
```

---

## 主题列表

### LED/Text - 文本显示

#### 消息格式

```json
{
  "target": "目标设备",
  "text": "要显示的文本",
  "scroll_mode": false,
  "font_size": 1,
  "color": "#FFFFFF",
  "line": 1,
  "wrap": true,
  "scroll_speed": 1,
  "scroll_direction": 0
}
```

#### 字段说明

| 字段 | 类型 | 必需 | 默认值 | 说明 |
|------|------|------|--------|------|
| target | string | 是 | - | 目标设备（精确或通配符） |
| text | string | 是 | - | 要显示的文本内容 |
| scroll_mode | boolean | 否 | false | 是否滚动模式 |
| font_size | int | 否 | 1 | 字体大小（1-2） |
| color | string | 否 | "#FFFFFF" | 文本颜色 |
| line | int | 否 | 1 | 行号（见下文行号限制） |
| wrap | boolean | 否 | true | 静态文本是否自动换行 |
| scroll_speed | int | 否 | 1 | 滚动速度（1=慢，2=中，3=快） |
| scroll_direction | int | 否 | 0 | 滚动方向（0=向左，1=向右） |

#### 行号限制（根据分辨率）

| 分辨率 | 最大行数 | 行号范围 |
|--------|----------|----------|
| 64x32 | 2 | 1-2 |
| 64x64 | 4 | 1-4 |
| 64x128 | 8 | 1-8 |

**计算公式**: `最大行数 = PANEL_RES_Y / 16`

#### 注意事项

⚠️ **重要**: `line`字段必须根据目标设备的分辨率设置，否则可能导致显示异常

1. **滚动模式** (`scroll_mode = true`):
   - `line` 必须在 `1` 到 `最大行数` 范围内
   - 不同字体大小的文本可以同时滚动
   - `wrap` 参数无效，强制为 `false`
   - 示例：64x64设备，`line` 可为 1, 2, 3, 4

2. **静态模式** (`scroll_mode = false`):
   - `line <= 0`（包括-1）时：清屏并全屏显示
   - `line > 0` 时：在指定行显示，不清屏
   - 如果有滚动文本存在，`wrap` 会被强制设为 `false`
   - 静态文本会自动清除重叠的滚动文本

3. **字体大小与覆盖**:
   - `font_size = 1`: 高度16像素，占1行
   - `font_size = 2`: 高度32像素，占2行


#### 使用示例

**示例1：向所有64x64设备发送滚动文本**
```json
{
  "target": "All_64x64",
  "text": "欢迎使用LED显示屏",
  "scroll_mode": true,
  "font_size": 1,
  "color": "#FFFFFF",
  "line": 1,
  "scroll_speed": 1,
  "scroll_direction": 0
}
```

**示例2：向指定设备发送静态文本**
```json
{
  "target": "ESP32/64x64/aabbccdd1122",
  "text": "Hello World",
  "scroll_mode": false,
  "font_size": 2,
  "color": "#FF0000",
  "line": 1,
  "wrap": false
}
```

**示例3：多行滚动文本（64x64设备）**
```json
{
  "target": "All_64x64",
  "text": "第1行 - 小字体",
  "scroll_mode": true,
  "font_size": 1,
  "line": 1,
  "scroll_speed": 1
}
```

```json
{
  "target": "All_64x64",
  "text": "第2行 - 大字体",
  "scroll_mode": true,
  "font_size": 2,
  "line": 2,
  "scroll_speed": 2
}
```

---

### LED/Clear - 清屏

#### 消息格式

```json
{
  "target": "目标设备",
  "clear": true
}
```

#### 字段说明

| 字段 | 类型 | 必需 | 默认值 | 说明 |
|------|------|------|--------|------|
| target | string | 是 | - | 目标设备（精确或通配符） |
| clear | boolean | 是 | true | 是否清屏 |

#### 注意事项

- `clear` 必须为 `true` 才会执行清屏操作
- 清屏会停止所有滚动文本并释放相关内存
- 无需考虑设备分辨率限制

#### 使用示例

**示例1：清空所有64x64设备的屏幕**
```json
{
  "target": "All_64x64",
  "clear": true
}
```

**示例2：清空指定设备的屏幕**
```json
{
  "target": "ESP32/64x32/aabbccdd1122",
  "clear": true
}
```

---

### LED/Brightness - 亮度设置

#### 消息格式

```json
{
  "target": "目标设备",
  "brightness": 128
}
```

#### 字段说明

| 字段 | 类型 | 必需 | 默认值 | 说明 |
|------|------|------|--------|------|
| target | string | 是 | - | 目标设备（精确或通配符） |
| brightness | int | 是 | 128 | 亮度值（0-255） |

#### 注意事项

- `brightness` 范围：0（最暗）到 255（最亮）
- 超出范围会自动限制在 0-255 之间
- 无需考虑设备分辨率限制

#### 使用示例

**示例1：设置所有设备亮度为128（中等亮度）**
```json
{
  "target": "All_64x64",
  "brightness": 128
}
```

**示例2：设置指定设备为最亮**
```json
{
  "target": "ESP32/64x64/aabbccdd1122",
  "brightness": 255
}
```

---

### LED/Image - 图片显示

#### 消息格式

```json
{
  "target": "目标设备",
  "image_base64": "base64编码的RGB565图像数据"
}
```

#### 字段说明

| 字段 | 类型 | 必需 | 默认值 | 说明 |
|------|------|------|--------|------|
| target | string | 是 | - | 目标设备（精确或通配符） |
| image_base64 | string | 是 | - | Base64编码的RGB565图像数据 |

#### 图像要求

| 项目 | 要求 |
|------|------|
| 颜色格式 | RGB565（每个像素2字节） |
| 图像尺寸 | 必须匹配目标设备的分辨率 |
| 编码方式 | Base64编码 |
| 数据大小 | 不得超过MAX_MQTT_PAYLOAD_SIZE（12800字节） |

#### Base64编码大小计算

对于64x64分辨率的屏幕：
- 原始大小：64 × 64 × 2 = 8192 字节
- Base64编码后大小：8192 × 4/3 ≈ 10923 字节（在限制范围内）

对于64x32分辨率的屏幕：
- 原始大小：64 × 32 × 2 = 4096 字节
- Base64编码后大小：4096 × 4/3 ≈ 5461 字节（在限制范围内）

#### 注意事项

⚠️ **重要**: 图片分辨率必须与目标设备分辨率完全匹配

- 64x64的图片只能发送给64x64的设备
- 64x32的图片只能发送给64x32的设备
- 不匹配的图片会被拒绝
- 显示图片时会自动停止所有滚动文本并清屏

#### 使用示例

**示例1：向所有64x64设备发送图片**
```json
{
  "target": "All_64x64",
  "image_base64": "AQIDBAUGBwgJCgsMDQ4PEBESExQVFhcYGRobHB0eHyAhIiMkJSYnKCkqKywtLi8wMTIzNDU2Nzg5Ojs8PT4/QEFCQ0RFRkdISUpLTE1OT1BRUlNUVVZXWFlaW1xdXl9gYWJjZGVmZ2hpamtsbW5vcHFyc3R1dnd4eXp7fH1+f4CBgoOEhYaHiImKi4yNjo+QkZKTlJWWl5iZmpucnZ6foKGio6SlpqeoqaqrrK2ur7CxsrO0tba3uLm6u7y9vr/AwcLDxMXGx8jJysvMzc7P0NHS09TV1tfY2drb3N3e3+Dh4uPk5ebn6Onq6+zt7u/w8fLz9PX29/j5+vv8/f7/"
}
```

**示例2：向指定设备发送图片**
```json
{
  "target": "ESP32/64x32/aabbccdd1122",
  "image_base64": "AQIDBAUGBwgJCgsMDQ4PEBESExQVFhcYGRobHB0eHyAhIiMkJSYnKCkqKywtLi8wMTIzNDU2Nzg5Ojs8PT4="
}
```

---

## 设备分辨率适配

### 支持的分辨率

| 分辨率 | 最大行数 | 通配符示例 |
|--------|----------|-----------|
| 64x32 | 2 | All_64x32 |
| 64x64 | 4 | All_64x64 |

### 如何判断设备分辨率

通过解析设备的`clientId`：

```
ESP32/{分辨率}/{MAC地址}
```

**解析示例：**
- `ESP32/64x64/aabbccdd1122` → 分辨率为 64x64
- `ESP32/64x32/aabbccdd1122` → 分辨率为 64x32

### 分辨率适配表

| 分辨率 | PANEL_RES_Y | 最大行数 | line有效范围 |
|--------|------------|----------|--------------|
| 64x32 | 32 | 2 | 1-2 |
| 64x64 | 64 | 4 | 1-4 |


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


---

## 消息限制

### 通用限制

| 限制项 | 值 | 说明 |
|--------|---|------|
| 最大消息大小 | 12800 字节 | 超过此大小的消息将被拒绝 |
| JSON文档大小 | 256 字节（Text主题） | 静态JSON文档大小限制 |
| Base64数据大小 | 12800 字节（Image主题） | 图片Base64数据大小限制 |

### 各主题具体限制

#### LED/Text

| 限制项 | 值 |
|--------|-----|
| JSON文档大小 | ≤ 256 字节 |
| 文本长度 | 无硬性限制（受JSON大小限制） |
| 行号范围 | 1 到 `PANEL_RES_Y / 16` |
| 字体大小 | 1-4 |

#### LED/Clear

| 限制项 | 值 |
|--------|-----|
| JSON文档大小 | ≤ 64 字节 |

#### LED/Brightness

| 限制项 | 值 |
|--------|-----|
| JSON文档大小 | ≤ 64 字节 |
| 亮度值范围 | 0-255 |

#### LED/Image

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
MQTT_SERVER = "zheng241.xyz"
MQTT_PORT = 1883
MQTT_USERNAME = "minggo"
MQTT_PASSWORD = "123456"

# 创建MQTT客户端
client = mqtt.Client()

# 设置用户名和密码
client.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD)

# 连接到MQTT服务器
client.connect(MQTT_SERVER, MQTT_PORT, 60)

# ========================================
# 示例1：向所有64x64设备发送滚动文本
# ========================================
text_msg = {
    "target": "All_64x64",
    "text": "欢迎使用LED显示屏",
    "scroll_mode": True,
    "font_size": 1,
    "color": "#FFFFFF",
    "line": 1,
    "scroll_speed": 1,
    "scroll_direction": 0
}
client.publish("LED/Text", json.dumps(text_msg))

# ========================================
# 示例2：向所有64x64设备发送多行滚动文本
# ========================================
# 第1行
text_msg1 = {
    "target": "All_64x64",
    "text": "第1行 - 小字体",
    "scroll_mode": True,
    "font_size": 1,
    "line": 1,
    "scroll_speed": 1
}
client.publish("LED/Text", json.dumps(text_msg1))

# 第2行（大字体）
text_msg2 = {
    "target": "All_64x64",
    "text": "第2行 - 大字体",
    "scroll_mode": True,
    "font_size": 2,
    "line": 2,
    "scroll_speed": 2
}
client.publish("LED/Text", json.dumps(text_msg2))

# ========================================
# 示例3：清空屏幕
# ========================================
clear_msg = {
    "target": "All_64x64",
    "clear": True
}
client.publish("LED/Clear", json.dumps(clear_msg))

# ========================================
# 示例4：设置亮度
# ========================================
brightness_msg = {
    "target": "All_64x64",
    "brightness": 128
}
client.publish("LED/Brightness", json.dumps(brightness_msg))

# ========================================
# 示例5：发送图片
# ========================================
image_msg = {
    "target": "All_64x64",
    "image_base64": "AQIDBAUGBwgJCgsMDQ4PEBESExQVFhcYGRobHB0eHyAhIiMkJSYnKCkqKywtLi8wMTIzNDU2Nzg5Ojs8PT4/QEFCQ0RFRkdISUpLTE1OT1BRUlNUVVZXWFlaW1xdXl9gYWJjZGVmZ2hpamtsbW5vcHFyc3R1dnd4eXp7fH1+f4CBgoOEhYaHiImKi4yNjo+QkZKTlJWWl5iZmpucnZ6foKGio6SlpqeoqaqrrK2ur7CxsrO0tba3uLm6u7y9vr/AwcLDxMXGx8jJysvMzc7P0NHS09TV1tfY2drb3N3e3+Dh4uPk5ebn6Onq6+zt7u/w8fLz9PX29/j5+vv8/f7/"
}
client.publish("LED/Image", json.dumps(image_msg))

# 断开连接
client.disconnect()
```



## 常见问题

### Q1：如何获取设备的ClientId？

设备的ClientId格式为 `ESP32/{分辨率}/{MAC地址}`，可通过后端获取。

### Q2：target字段可以省略吗？

不可以。所有消息都必须包含`target`字段，否则消息会被拒绝。

### Q3：如何向所有设备发送消息？

使用通配符格式，但只能向相同分辨率的设备发送消息。

示例：
- `"target": "All_64x64"` - 向所有64x64设备发送
- `"target": "All_64x32"` - 向所有64x32设备发送

### Q4：不同分辨率的设备如何管理？

建议为不同分辨率的设备使用不同的通配符：
- 64x64设备：`All_64x64`
- 64x32设备：`All_64x32`

这样可以确保行号限制正确，避免显示异常。

### Q5：为什么64x32设备的line=3无效？

64x32分辨率的屏幕最大行数为2（`32 / 16 = 2`），所以line的有效范围是1-2。line=3会超出限制，可能导致显示异常。

### Q6：图片消息的大小限制是多少？

Base64编码后的图片数据不得超过12800字节。对于64x64分辨率的屏幕，Base64编码后约为10923字节，在限制范围内。

### Q7：如何发送不同字体大小的滚动文本？

只需在发送消息时设置不同的`font_size`即可。不同字体大小的文本可以同时滚动。

```python
# 第1行：小字体
text_msg1 = {
    "target": "All_64x64",
    "text": "小字体",
    "scroll_mode": True,
    "font_size": 1,
    "line": 1
}

# 第2行：大字体
text_msg2 = {
    "target": "All_64x64",
    "text": "大字体",
    "scroll_mode": True,
    "font_size": 2,
    "line": 2
}
```

### Q8：静态文本的wrap参数有什么作用？

- `wrap = true`: 文本自动换行，适合长文本
- `wrap = false`: 文本不自动换行，超出部分不显示

**注意**：如果有滚动文本存在，`wrap`会被强制设为`false`，以避免冲突。前端只有当滚动模式未开启即静态文本时，才显示wrap选项。

### Q9：如何停止所有滚动文本？

发送清屏消息：
```json
{
  "target": "目标设备或通配符",
  "clear": true
}
```

### Q10：消息发送失败怎么办？

1. 检查MQTT服务器连接是否正常
2. 检查target字段是否正确
3. 检查消息大小是否超过限制
4. 检查JSON格式是否正确
5. 查看设备串口输出获取错误信息

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

| 速度等级 | 说明 | 像素偏移/帧 |
|---------|------|------------|
| 1 | 慢速 | 1 像素 |
| 2 | 中速 | 2 像素 |
| 3 | 快速 | 3 像素 |

### 字体大小参考

| 字体大小 | 高度（像素） | 说明 |
|---------|------------|------|
| 1 | 16 | 小字体 |
| 2 | 32 | 大字体 |


---

## 总结

使用MQTT客户端控制LED显示屏的关键点：

1. **target字段**: 所有消息都必须包含，用于指定目标设备
2. **分辨率适配**: 根据设备分辨率限制line等参数的范围
3. **通配符支持**: 使用`All_{宽度}x{高度}`格式控制同分辨率的所有设备
4. **消息大小**: 注意消息大小限制，避免超过12800字节
5. **错误处理**: 查看设备串口输出获取详细的错误信息

遵循本文档的说明，可以正确、高效地控制LED矩阵显示屏。
