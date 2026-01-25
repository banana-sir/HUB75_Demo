# ESP32 HUB75 LED Matrix Display

基于PlatformIO和Arduino框架的ESP32 HUB75 LED矩阵显示项目。

## 项目结构

```
├── include/
│   ├── config.h              # 配置文件
│   ├── display_module.h      # 显示模块头文件
│   └── serial_module.h       # 串口模块头文件
├── lib/
│   ├── display_module/
│   │   └── display_module.cpp  # 显示模块实现
│   └── serial_module/
│       └── serial_module.cpp   # 串口模块实现
├── src/
│   └── main.cpp              # 主程序
├── platformio.ini            # PlatformIO配置
└── serial_test.py            # Python测试脚本
```

## 模块说明

### display_module - 显示模块

提供LED矩阵显示的核心功能：
- 静态文本显示
- 滚动文本显示（支持指定行）
- 文本大小、颜色设置
- 滚动速度控制
- 屏幕清除

主要函数：
```c
void initDisplay(MatrixPanel_I2S_DMA *display);
void displayText(const char *textContent, bool isScroll);
void displayText(const char *textContent, bool isScroll, int line);
void updateScrollAnimation(void);
void setTextColor(uint16_t color);
void setTextSize(int size);
void setTextScrollSpeed(int speed);
void clearAll(void);
```

### serial_module - 串口模块

提供通过串口控制LED矩阵的功能：
- 命令格式简单易用
- 支持中文UTF-8编码

## 串口命令格式

| 命令 | 说明 | 示例 |
|------|------|------|
| `text:<内容>` | 静态显示文本 | `text:Hello World` |
| `scroll:<内容>` | 全屏滚动文本 | `scroll:Welcome` |
| `scroll2:<内容>` | 第2行滚动文本 | `scroll2:Line 2` |
| `<任意文本>` | 默认静态显示 | `Hello` |

## 使用方法

### 1. 编译上传

```bash
pio run --target upload
```

### 2. 串口测试

使用Python测试脚本（需安装pyserial）：
```bash
pip install pyserial
python serial_test.py
```

或使用串口调试工具（如Arduino IDE串口监视器）直接发送命令。

## 配置说明

编辑 `include/config.h` 修改：

```c
// LED矩阵配置
#define PANEL_RES_X 64      // 宽度
#define PANEL_RES_Y 32      // 高度
#define PANEL_CHAIN 1       // 链接长度

// GPIO引脚定义
#define R1_PIN 6
#define G1_PIN 7
// ... 其他引脚

// 默认亮度
#define DEFAULT_BRIGHTNESS 128

// 滚动速度配置
#define SCROLL_TIME_DELAY_LOW 30
#define SCROLL_OFFSET_LOW -1
// ... 其他速度配置
```

## 示例

### 显示静态文本

```c
displayText("Hello World", false);
```

### 显示滚动文本

```c
displayText("Scrolling text", true);          // 全屏滚动
displayText("Line 2", true, 2);                // 第2行滚动
```

### 通过串口控制

串口发送：`scroll2:Welcome to ESP32`

## 注意事项

1. 32像素高度最多支持2行文本
2. 中文文本需确保UTF-8编码
3. 滚动文本会自动循环播放
4. 串口波特率固定为115200

## 硬件连接

- ESP32 -> HUB75 LED矩阵
- 使用I2S DMA驱动，减少CPU占用
- GPIO引脚可在config.h中自定义

## 依赖库

- ESP32 HUB75 LED MATRIX PANEL DMA Display
- Adafruit GFX Library
