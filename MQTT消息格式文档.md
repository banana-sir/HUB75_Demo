# ESP32 LED点阵屏 MQTT消息格式文档

## 概述

本文档描述了向ESP32 LED点阵屏发送MQTT控制消息的JSON格式。所有消息都需要包含 `target` 字段来指定目标设备。

---

## 设备标识

### ClientId 格式
每个设备有一个唯一的 `clientId`，格式为：
```
ESP32/{宽度}x{高度}/{MAC地址}
```

例如：
- `ESP32/64x64/a1b2c3d4e5f6`

### 通配符
使用通配符可以向所有相同分辨率的设备发送消息：
```
All_{宽度}x{高度}
```

例如：
- `All_64x64` （发送给所有64x64分辨率的设备）
- `All_64x32` （发送给所有64x32分辨率的设备）

---

## MQTT主题列表

| 主题 | 用途 |
|------|------|
| `LED/Text` | 发送文本消息 |
| `LED/Clear` | 清除屏幕内容 |
| `LED/Brightness` | 调整亮度 |
| `LED/Image` | 发送图片消息 |

---

## 消息格式

### 1. 文本消息 (LED/Text)

显示静态文本或滚动文本。

#### JSON格式
```json
{
  "target": "ESP32/64x64/a1b2c3d4e5f6",
  "text": "Hello World",
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

| 字段 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| `target` | string | 是 | - | 目标设备ID或通配符 |
| `text` | string | 是 | - | 要显示的文本内容（支持UTF-8中文） |
| `scroll_mode` | boolean | 否 | false | 是否滚动显示 |
| `font_size` | integer | 否 | 1 | 字体大小（1=小，2=大） |
| `color` | string | 否 | "#FFFFFF" | 文本颜色（十六进制RGB格式） |
| `line` | integer | 否 | 1 | 文本显示行号（静态动态文本都需要此参数，决定在哪一行开始显示。根据分辨率和字体大小计算显示的选项数量，已知1号字体ASCII码字符大小为8*16像素，中文字符为16*16像素，2号字体则大小翻倍。如分辨率为64x32，字体大小为1，则有两个选项可选，即第1行和第2行，若大小为2则只有1行可选；若为64x64，字体大小为1，则有4行可显示，以此类推）
 |
| `wrap` | boolean | 否 | true | 静态文本是否自动换行 |
| `scroll_speed` | integer | 否 | 1 | 滚动速度（1=慢，2=中，3=快） |
| `scroll_direction` | integer | 否 | 0 | 滚动方向（0=向左，1=向右） |

#### 示例

**显示静态文本（自动换行）**
```json
{
  "target": "All_64x64",
  "text": "欢迎使用LED点阵屏",
  "scroll_mode": false,
  "font_size": 1,
  "color": "#00FF00",
  "line": 1,
  "wrap": true
}
```

**显示滚动文本**
```json
{
  "target": "ESP32/64x64/a1b2c3d4e5f6",
  "text": "这是一个滚动消息示例",
  "scroll_mode": true,
  "font_size": 2,
  "color": "#FF0000",
  "scroll_speed": 2,
  "scroll_direction": 0
}
```

---

### 2. 清除屏幕 (LED/Clear)

清除LED屏幕上的所有内容。

#### JSON格式
```json
{
  "target": "All_64x64",
  "clear": true
}
```

#### 字段说明

| 字段 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| `target` | string | 是 | - | 目标设备ID或通配符 |
| `clear` | boolean | 是 | - | 是否清除（必须为true才执行清除） |

#### 示例

**清除所有64x64设备的屏幕**
```json
{
  "target": "All_64x64",
  "clear": true
}
```

**清除指定设备的屏幕**
```json
{
  "target": "ESP32/64x64/a1b2c3d4e5f6",
  "clear": true
}
```

---

### 3. 亮度调整 (LED/Brightness)

调整LED屏幕的显示亮度。

#### JSON格式
```json
{
  "target": "ESP32/64x64/a1b2c3d4e5f6",
  "brightness": 128
}
```

#### 字段说明

| 字段 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| `target` | string | 是 | - | 目标设备ID或通配符 |
| `brightness` | integer | 否 | 128 | 亮度值（0-255，0=最暗，255=最亮） |

#### 示例

**设置亮度为中等（128）**
```json
{
  "target": "All_64x64",
  "brightness": 128
}
```

**设置亮度为最亮（255）**
```json
{
  "target": "ESP32/64x64/a1b2c3d4e5f6",
  "brightness": 255
}
```

**关闭屏幕（亮度为0）**
```json
{
  "target": "All_64x64",
  "brightness": 0
}
```

---

### 4. 图片消息 (LED/Image)

发送图片到LED屏幕显示。

#### JSON格式
```json
{
  "target": "ESP32/64x64/a1b2c3d4e5f6",
  "image_base64": "iVBORw0KGgoAAAANSUhEUgAA..."
}
```

#### 字段说明

| 字段 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| `target` | string | 是 | - | 目标设备ID或通配符 |
| `image_base64` | string | 是 | - | 图片的Base64编码字符串 |

#### 图片要求

- **格式**: 支持PNG、JPEG等常见格式
- **尺寸**: 建议与LED屏幕分辨率一致（如64x64）
- **Base64编码**: 使用标准的Base64编码，不包含 `data:image/png;base64,` 前缀

#### 示例

**发送图片到所有64x64设备**
```json
{
  "target": "All_64x64",
  "image_base64": "iVBORw0KGgoAAAANSUhEUgAA..."
}
```


## 完整示例

### JavaScript发送消息示例

```javascript
// 假设使用MQTT.js库
import mqtt from 'mqtt';

// 连接到MQTT服务器
const client = mqtt.connect('mqtt://zheng241.xyz:1883', {
  username: 'minggo',
  password: '123456'
});

client.on('connect', () => {
  console.log('已连接到MQTT服务器');

  // 示例1：发送文本消息
  const textMessage = {
    target: 'All_64x64',
    text: '欢迎使用LED点阵屏',
    scroll_mode: false,
    font_size: 1,
    color: '#FFFFFF',
    line: 1,
    wrap: true
  };
  client.publish('LED/Text', JSON.stringify(textMessage));

  // 示例2：清除屏幕
  const clearMessage = {
    target: 'All_64x64',
    clear: true
  };
  client.publish('LED/Clear', JSON.stringify(clearMessage));

  // 示例3：调整亮度
  const brightnessMessage = {
    target: 'All_64x64',
    brightness: 200
  };
  client.publish('LED/Brightness', JSON.stringify(brightnessMessage));

  // 示例4：发送图片
  const imageMessage = {
    target: 'All_64x64',
    image_base64: base64ImageString  // 替换为实际的Base64字符串
  };
  client.publish('LED/Image', JSON.stringify(imageMessage));
});
```


