# HUB75 LED 矩阵显示屏驱动系统

基于 ESP32-S3 和 HUB75 接口的 LED 矩阵显示系统，支持多行滚动文本、静态文本和图片显示，通过 MQTT 协议进行远程控制。

## 目录

- [项目简介](#项目简介)
- [硬件配置](#硬件配置)
- [功能特性](#功能特性)
- [系统架构](#系统架构)
- [快速开始](#快速开始)
- [MQTT 控制](#mqtt-控制)
- [配置说明](#配置说明)
- [开发文档](#开发文档)
- [技术栈](#技术栈)
- [许可证](#许可证)

---

## 项目简介

本项目是一个完整的 LED 矩阵显示系统，适用于 64x32 或 64x64 分辨率的 HUB75 接口 LED 屏幕模块。系统采用 ESP32-S3 双核架构，通过 MQTT 协议实现远程控制，支持多种显示模式。

### 主要特点

- 🚀 **高性能**：双核 ESP32-S3，50fps 刷新率，DMA 双缓冲无闪烁
- 📱 **远程控制**：基于 MQTT 协议的远程消息推送和控制
- 🎨 **多模式显示**：支持静态文本、多行滚动文本、RGB565 图片
- 🌐 **智能联网**：自动 WiFi 配置和 MQTT 连接
- 🛡️ **稳定可靠**：看门狗保护、错误恢复、消息队列异步处理
- 📊 **可扩展**：模块化设计，易于扩展新功能

---

## 硬件配置

### 支持的屏幕规格

| 规格 | 分辨率 | 最大行数 | GPIO 引脚配置 |
|------|--------|----------|---------------|
| 64x32 | 64 x 32 | 2 行 | 见 config.h (LED_SIZE=0) |
| 64x64 | 64 x 64 | 4 行 | 见 config.h (LED_SIZE=1) |

### ESP32-S3 开发板要求

- **开发板型号**：ESP32-S3-DevKitC-1
- **CPU**：双核 240MHz
- **内存**：512KB SRAM
- **Flash**：至少 4MB

### HUB75 接口连接

屏幕模块通过 HUB75 接口与 ESP32-S3 连接，引脚配置在 `include/config.h` 中定义：

```cpp
// 64x32 屏幕引脚配置
R1_PIN  = 18   // 红色数据高位
G1_PIN  = 38   // 绿色数据高位
B1_PIN  = 17   // 蓝色数据高位
R2_PIN  = 16   // 红色数据低位
G2_PIN  = 39   // 绿色数据低位
B2_PIN  = 15   // 蓝色数据低位
A_PIN   = 7    // 行选择 A
B_PIN   = 41   // 行选择 B
C_PIN   = 6    // 行选择 C
D_PIN   = 2    // 行选择 D
LAT_PIN = 1    // 锁存信号
OE_PIN  = 4    // 输出使能
CLK_PIN = 5    // 时钟信号
```

---

## 功能特性

### 1. 文本显示

#### 静态文本
- 支持多行静态文本显示
- 自动换行功能
- 多种字体大小（size=1 或 size=2）
- 自定义颜色（RGB565 格式）

#### 滚动文本
- 多行同时滚动（最多 2-4 行）
- 每行独立控制：字体大小、颜色、速度、方向
- 双向滚动（向左/向右）
- 三档速度调节（慢速/中速/快速）
- 自动冲突检测和清除

### 2. 图片显示
- Base64 编码的 RGB565 图片
- 完整屏幕显示
- 自动退出图片模式接收新内容

### 3. 亮度控制
- 动态亮度调节（0-255）
- 远程实时控制

### 4. 清屏功能
- 一键清空所有显示内容
- 退出图片模式

### 5. 显示优化
- DMA 双缓冲机制（50fps，无闪烁）
- 静态文本与滚动文本完美共存
- 自动重叠检测和清除
- 高效内存管理

---

## 系统架构

### 双核任务分配

```
┌─────────────────────────────────────────────────────────┐
│                    ESP32-S3 双核架构                     │
├─────────────────────────────────────────────────────────┤
│                                                          │
│  ┌─────────────────────┐    ┌─────────────────────┐    │
│  │    Core 0           │    │    Core 1           │    │
│  │ (WiFi/MQTT 任务)    │    │ (显示任务)          │    │
│  ├─────────────────────┤    ├─────────────────────┤    │
│  │ • WiFi 连接管理     │    │ • 显示初始化        │    │
│  │ • MQTT 连接维护     │    │ • 帧渲染循环        │    │
│  │ • 消息接收回调      │    │ • 滚动文本更新      │    │
│  │ • 消息队列处理      │    │ • DMA 缓冲区翻转    │    │
│  └─────────────────────┘    └─────────────────────┘    │
│            │                          │                 │
│            │ 消息队列                 │                 │
│            └──────────┬───────────────┘                 │
│                       │                                 │
│               ┌───────▼───────┐                         │
│               │  消息队列缓冲 │                         │
│               │ (FIFO)        │                         │
│               └───────────────┘                         │
│                                                          │
└─────────────────────────────────────────────────────────┘
```

### 模块结构

```
HUB75_Demo/
├── src/
│   └── main.cpp              # 主程序入口
├── include/
│   └── config.h              # 全局配置文件
├── lib/
│   ├── DisplayManager/
│   │   ├── DisplayManager.h  # 显示管理器头文件
│   │   └── DisplayManager.cpp # 显示管理器实现
│   └── WiFiManager/
│       ├── WiFiManager.h     # WiFi/MQTT管理器头文件
│       └── WiFiManager.cpp   # WiFi/MQTT管理器实现
├── platformio.ini            # PlatformIO 配置
└── README.md                 # 项目说明文档
```

### 消息处理流程

```
MQTT 服务器
    ↓ (MQTT 消息)
WiFi/MQTT 回调 (Core 0)
    ↓
消息队列 (FIFO 缓冲)
    ↓
消息处理任务 (Core 0)
    ↓
DisplayManager (Core 1)
    ↓
LED 矩阵显示
```

---

## 快速开始

### 环境要求

- **开发环境**：PlatformIO IDE for VSCode
- **框架**：Arduino Framework for ESP32
- **串口驱动**：CP210x 或 CH340 USB 驱动

### 安装依赖

项目依赖库已在 `platformio.ini` 中配置，PlatformIO 会自动下载：

```ini
lib_deps =
    mrfaptastic/ESP32 HUB75 LED MATRIX PANEL DMA Display@^3.0.14
    adafruit/Adafruit GFX Library@1.12.0
    bblanchon/ArduinoJson@^6.21.3
    knolleary/PubSubClient@^2.8
    densaugeo/base64@^1.4.0
```

### 编译和上传

1. **克隆或下载项目**
   ```bash
   cd HUB75_Demo
   ```

2. **配置串口端口**
   
   修改 `platformio.ini` 中的 `monitor_port`：
   ```ini
   monitor_port = COM15  # Windows
   # 或
   monitor_port = /dev/ttyUSB0  # Linux
   ```

3. **编译项目**
   ```bash
   pio run
   ```

4. **上传到设备**
   ```bash
   pio run --target upload
   ```

5. **打开串口监视器**
   ```bash
   pio device monitor
   ```

### 配置屏幕尺寸

在 `include/config.h` 中修改 `LED_SIZE`：

```cpp
#define LED_SIZE 0   // 0: 64x32, 1: 64x64
```

修改后会自动调整：
- 分辨率设置
- GPIO 引脚配置
- MQTT 用户名（64x32esp32 或 64x64esp32）
- 最大行数（2 行或 4 行）

---

## MQTT 控制

### MQTT 服务器配置

默认配置：
- **服务器**：zheng221.xyz
- **端口**：1883
- **用户名**：64x32esp32 或 64x64esp32（根据屏幕尺寸）
- **密码**：123456

### MQTT 主题格式

所有主题基于设备 MAC 地址动态生成：

```
LED/{MAC地址}/{功能}
```

例如：
```
LED/246f28a58ec4/Text      # 文本显示
LED/246f28a58ec4/Clear     # 清屏
LED/246f28a58ec4/Brightness # 亮度设置
LED/246f28a58ec4/Image     # 图片显示
```

### 消息格式

#### 1. 文本显示

**主题**：`LED/{MAC}/Text`

**消息格式**（JSON）：
```json
{
  "text": "Hello World!",
  "scroll_mode": true,
  "font_size": 1,
  "color": "#FFFFFF",
  "line": 1,
  "wrap": true,
  "scroll_speed": 2,
  "scroll_direction": 0
}
```

**参数说明**：

| 参数 | 类型 | 说明 | 可选值 |
|------|------|------|--------|
| text | string | 显示文本（支持 UTF-8 中文） | 必填 |
| scroll_mode | boolean | 是否滚动文本 | true/false |
| font_size | int | 字体大小 | 1（小）或 2（大） |
| color | string | 文本颜色（十六进制） | #FFFFFF 等 |
| line | int | 行号（1-maxLines） | 1-4 |
| wrap | boolean | 是否自动换行（仅静态文本） | true/false |
| scroll_speed | int | 滚动速度 | 1（慢）、2（中）、3（快） |
| scroll_direction | int | 滚动方向 | 0（向左）、1（向右） |

**示例**：
```json
// 静态文本
{
  "text": "欢迎使用LED显示屏",
  "scroll_mode": false,
  "font_size": 2,
  "color": "#FF0000",
  "line": -1,
  "wrap": true
}

// 滚动文本
{
  "text": "这是一条滚动消息",
  "scroll_mode": true,
  "font_size": 1,
  "color": "#00FF00",
  "line": 1,
  "scroll_speed": 2,
  "scroll_direction": 0
}
```

#### 2. 清屏

**主题**：`LED/{MAC}/Clear`

**消息格式**（JSON）：
```json
{
  "clear": true
}
```

#### 3. 亮度设置

**主题**：`LED/{MAC}/Brightness`

**消息格式**（JSON）：
```json
{
  "brightness": 128
}
```

**参数说明**：
- brightness：亮度值（0-255）

#### 4. 图片显示

**主题**：`LED/{MAC}/Image`

**消息格式**（JSON）：
```json
{
  "image_base64": "...base64编码的RGB565图片数据..."
}
```

**注意事项**：
- 图片必须为 RGB565 格式（每像素 2 字节）
- 图片尺寸必须与屏幕分辨率一致（64x32 或 64x64）
- Base64 编码后的数据长度不超过 12800 字节
- 显示图片后会进入图片模式，发送其他消息会自动退出

### 使用 MQTT 客户端测试

#### 使用 mosquitto_pub

```bash
# 显示静态文本
mosquitto_pub -h zheng221.xyz -p 1883 \
  -u 64x32esp32 -P 123456 \
  -t "LED/246f28a58ec4/Text" \
  -m '{"text":"Hello","scroll_mode":false,"font_size":1,"color":"#FFFFFF","line":1}'

# 显示滚动文本
mosquitto_pub -h zheng221.xyz -p 1883 \
  -u 64x32esp32 -P 123456 \
  -t "LED/246f28a58ec4/Text" \
  -m '{"text":"滚动消息","scroll_mode":true,"font_size":1,"color":"#00FF00","line":1,"scroll_speed":2,"scroll_direction":0}'

# 设置亮度
mosquitto_pub -h zheng221.xyz -p 1883 \
  -u 64x32esp32 -P 123456 \
  -t "LED/246f28a58ec4/Brightness" \
  -m '{"brightness":200}'

# 清屏
mosquitto_pub -h zheng221.xyz -p 1883 \
  -u 64x32esp32 -P 123456 \
  -t "LED/246f28a58ec4/Clear" \
  -m '{"clear":true}'
```

#### 使用 MQTT.fx / MQTT Explorer

1. 连接配置：
   - 服务器：zheng221.xyz
   - 端口：1883
   - 用户名：64x32esp32（或 64x64esp32）
   - 密码：123456

2. 发布消息：
   - 主题：`LED/{MAC地址}/{功能}`
   - 消息：JSON 格式数据

---

## 配置说明

### 调试模式

在 `include/config.h` 中控制调试输出：

```cpp
#define DEBUG_MODE 1  // 0: 关闭, 1: 开启
```

开启后会通过串口输出详细的调试信息。

### 滚动速度配置

在 `include/config.h` 中配置滚动速度：

```cpp
#define SCROLL_INTERVAL_SLOW 45      // 慢速：每45ms移动1像素
#define SCROLL_INTERVAL_MEDIUM 35    // 中速：每35ms移动1像素
#define SCROLL_INTERVAL_FAST 20     // 快速：每20ms移动1像素
```

### MQTT 配置

在 `include/config.h` 中配置 MQTT 连接：

```cpp
#define MQTT_SERVER "zheng221.xyz"
#define MQTT_PORT 1883
#define MQTT_USERNAME "64x32esp32"  // 或 "64x64esp32"
#define MQTT_PASSWORD "123456"
#define MAX_MQTT_PAYLOAD_SIZE 12800  // 最大消息载荷
```

### WiFi 配置

WiFi 配置在 `lib/WiFiManager/WiFiManager.cpp` 中：

```cpp
const char* ssid = "YourWiFiSSID";
const char* password = "YourWiFiPassword";
```

---

## 开发文档

项目包含详细的开发文档，帮助理解和扩展功能：

1. **[DisplayManager 使用文档.md](DisplayManager使用文档.md)**
   - DisplayManager API 参考
   - DMA 双缓冲机制详解
   - 静态文本与滚动文本共存原理
   - 图片显示功能说明

2. **[MQTT 客户端使用文档.md](MQTT客户端使用文档.md)**
   - MQTT 协议配置
   - 消息格式说明
   - 主题结构详解
   - 常见问题解答

3. **[MQTT 消息回调优化说明.md](MQTT消息回调优化说明.md)**
   - 异步消息队列设计
   - 双核任务架构
   - 性能优化策略
   - 看门狗保护机制

4. **[多行滚动文本实现文档.md](多行滚动文本实现文档.md)**
   - 滚动文本核心设计
   - 多字体共存机制
   - Y 坐标系统
   - 重叠检测算法
   - 渲染流程详解

---

## 技术栈

### 硬件
- **微控制器**：ESP32-S3 (双核 Xtensa LX7 @ 240MHz)
- **显示屏**：HUB75 接口 LED 矩阵（64x32 或 64x64）

### 软件框架
- **开发平台**：PlatformIO
- **框架**：Arduino Framework for ESP32
- **操作系统**：FreeRTOS（双核任务调度）

### 核心库
- **ESP32 HUB75 LED Matrix Panel DMA Display**：LED 驱动，DMA 双缓冲
- **Adafruit GFX Library**：图形绘制
- **ArduinoJson**：JSON 解析
- **PubSubClient**：MQTT 客户端
- **base64**：Base64 编码/解码

### 编程语言
- **C++**：主要开发语言
- **Arduino C++**：框架 API

---

## 项目结构

```
HUB75_Demo/
├── src/                    # 源代码
│   └── main.cpp           # 主程序入口
├── include/               # 头文件
│   └── config.h           # 全局配置
├── lib/                   # 自定义库
│   ├── DisplayManager/    # 显示管理器
│   │   ├── DisplayManager.h
│   │   └── DisplayManager.cpp
│   └── WiFiManager/       # WiFi/MQTT管理器
│       ├── WiFiManager.h
│       └── WiFiManager.cpp
├── test/                  # 测试文件
├── .gitignore            # Git 忽略配置
├── platformio.ini        # PlatformIO 配置
├── README.md             # 项目说明（本文档）
├── DisplayManager使用文档.md
├── MQTT客户端使用文档.md
├── MQTT消息回调优化说明.md
└── 多行滚动文本实现文档.md
```

---

## 常见问题

### 1. 屏幕不显示

**检查项**：
- GPIO 引脚连接是否正确
- 屏幕电源是否正常
- `LED_SIZE` 配置是否正确
- 是否调用 `displayManager.init()`

### 2. 无法连接 WiFi

**检查项**：
- WiFi 名称和密码是否正确
- 路由器是否支持 2.4GHz
- 是否在代码中配置了 WiFi

### 3. 无法连接 MQTT

**检查项**：
- MQTT 服务器地址和端口是否正确
- 用户名和密码是否正确
- 网络连接是否正常
- MQTT 服务器是否在线

### 4. MQTT 消息无响应

**检查项**：
- 主题格式是否正确（包含 MAC 地址）
- JSON 格式是否正确
- 打开调试模式查看日志

### 5. 图片显示不正常

**检查项**：
- 图片尺寸是否与屏幕一致
- 是否为 RGB565 格式
- Base64 编码是否正确
- 数据长度是否超限

---

## 性能指标

### 系统性能

| 指标 | 数值 | 说明 |
|------|------|------|
| 刷新率 | 50 fps | DMA 双缓冲驱动 |
| 帧间隔 | 20 ms | 稳定刷新 |
| 渲染时间 | 1-3 ms | 远低于帧间隔 |
| 滚动速度 | 20-45 ms/像素 | 三档可调 |
| 最大消息载荷 | 12800 字节 | MQTT 限制 |

### 内存使用

| 组件 | 估计内存 | 说明 |
|------|----------|------|
| WiFi/MQTT 任务堆栈 | 16 KB | 固定分配 |
| 显示缓冲区 | 8 KB | 64x64x2 RGB565 |
| 消息队列 | ~5 KB | 最多 10 条消息 |
| 滚动文本数据 | ~1 KB | 动态分配 |

---

## 扩展开发

### 添加新的显示功能

1. 在 `DisplayManager.h` 中声明新函数
2. 在 `DisplayManager.cpp` 中实现功能
3. 在 `WiFiManager.cpp` 中添加 MQTT 消息处理
4. 更新文档说明

### 添加新的 MQTT 主题

1. 定义新的主题常量
2. 在消息处理函数中添加 case
3. 调用相应的 DisplayManager 函数
4. 更新文档说明

### 修改滚动速度

编辑 `include/config.h` 中的间隔参数：
```cpp
#define SCROLL_INTERVAL_SLOW 50    // 修改慢速
#define SCROLL_INTERVAL_MEDIUM 35 // 修改中速
#define SCROLL_INTERVAL_FAST 20   // 修改快速
```

---

## 贡献指南

欢迎贡献代码、报告问题或提出建议！

### 报告问题

请在 GitHub Issues 中提供：
- 问题描述
- 复现步骤
- 环境信息（硬件、配置）
- 相关日志

### 提交代码

1. Fork 本项目
2. 创建特性分支
3. 提交更改
4. 推送到分支
5. 创建 Pull Request

---

## 许可证

本项目仅供学习和研究使用。

---

## 联系方式

如有问题或建议，欢迎通过以下方式联系：

- 提交 GitHub Issue
- 发送邮件至项目维护者

---

## 更新日志

### v1.0 (2026)
- ✅ 初始版本发布
- ✅ 支持静态文本和滚动文本
- ✅ 支持 Base64 图片显示
- ✅ MQTT 远程控制
- ✅ 双核任务架构
- ✅ DMA 双缓冲机制
- ✅ 看门狗保护
- ✅ 异步消息队列

---

**祝您使用愉快！** 🎉
