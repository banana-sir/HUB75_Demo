# DisplayManager 使用文档

## 概述

DisplayManager 是用于控制 HUB75 LED 矩阵显示屏的管理类，支持静态文本显示、多行滚动文本显示和 Base64 RGB565 图像显示。

### 核心特性

- ✅ 多字体大小同时滚动（每行独立控制）
- ✅ 自动冲突检测和清除
- ✅ 支持静态文本和滚动文本混合显示
- ✅ 支持自动换行（静态文本）
- ✅ 双向滚动（向左/向右）
- ✅ 多级滚动速度控制
- ✅ 图片显示模式（Base64 RGB565）
- ✅ 50fps 高刷新率（DMA双缓冲）
- ✅ 静态文本与滚动文本完美共存
- ✅ 双缓冲机制确保无闪烁显示

---

## DMA双缓冲机制

### 双缓冲工作原理

DisplayManager采用DMA（Direct Memory Access）双缓冲技术，这是实现高刷新率和无闪烁显示的核心机制。

```
┌─────────────────────────────────────────────────────────────┐
│                    DMA双缓冲系统                              │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌──────────────┐         ┌──────────────┐                 │
│  │ 前缓冲区     │         │ 后缓冲区     │                 │
│  │ (Front Buffer)│       │ (Back Buffer) │                 │
│  │              │         │              │                 │
│  │  当前显示中   │         │  正在绘制中   │                 │
│  │              │         │              │                 │
│  └──────┬───────┘         └──────┬───────┘                 │
│         │                        │                          │
│         │ DMA持续输出             │                          │
│         │ 到LED矩阵              │                          │
│         │                        │                          │
│         │                        │ 渲染操作                 │
│         │                        │ (clearScreen/draw)     │
│         │                        │                          │
│         │         flipDMABuffer() │                          │
│         └────────────────────────┘                          │
│                  交换缓冲区                                  │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### 双缓冲的优势

1. **无闪烁显示**
   - 前缓冲区持续显示，不会被修改
   - 后缓冲区独立绘制，不干扰当前显示
   - 翻转时交换指针，瞬间完成

2. **高刷新率**
   - 50fps稳定刷新（frameInterval = 20ms）
   - DMA硬件驱动，释放CPU资源
   - 不阻塞主循环执行

3. **平滑动画**
   - 滚动文本流畅移动
   - 无撕裂或闪烁现象
   - 视觉体验稳定

### 渲染流程

#### 完整帧渲染时序

```
时间轴：0ms    20ms   40ms   60ms   80ms   100ms
        │      │      │      │      │       │
        ▼      ▼      ▼      ▼      ▼       ▼
        ┌─────────────────────────────────────┐
        │ frameInterval = 20ms (50fps)       │
        └─────────────────────────────────────┘

Frame 1: 0ms ~ 20ms
  ├── updateScrolling()  更新所有滚动文本的位置
  └── renderFrame()      渲染完整帧
      ├── clearScreen()      清除后缓冲区（填充黑色）
      ├── renderStaticTexts()   绘制静态文本层
      ├── renderScrollingTexts() 绘制滚动文本层
      └── flipDMABuffer()    交换前后缓冲区指针

Frame 2: 20ms ~ 40ms
  ├── updateScrolling()
  └── renderFrame()
      ...

Frame 3: 40ms ~ 60ms
  ...
```

#### 代码实现

```cpp
void DisplayManager::loop() {
    // 如果是图片显示模式或全屏显示模式或没有内容可显示，跳过渲染循环
    // 全屏静态文本(line=-1)采用非渲染模式，类似于图片显示，提高效率
    if (isImageDisplayMode || isFullScreenDisplay || !hasContentToDisplay) {
        return;
    }

    unsigned long now = millis();

    // 帧率控制：每20ms渲染一帧
    if (now - lastFrameTime >= frameInterval) {
        updateScrolling();  // 更新滚动位置
        renderFrame();      // 渲染并翻转缓冲区
        lastFrameTime = now;
    }
}
```

### 静态文本与滚动文本共存

#### 渲染层次结构

```
渲染层次（从底到顶）：

Layer 0: 背景层
  └─ clearScreen() 填充整个后缓冲区为黑色

Layer 1: 静态文本层
  └─ renderStaticTexts() 绘制所有静态文本
     └─ 按照 line 顺序绘制
     └─ 支持多行不同字体大小

Layer 2: 滚动文本层
  └─ renderScrollingTexts() 绘制所有滚动文本
     ├─ 每行绘制前先清除该行区域
     ├─ 支持每行独立字体大小、颜色、速度、方向
     └─ 按照更新位置绘制文本
```

#### 关键设计要点

1. **先静态后滚动**
   - 静态文本先绘制到后缓冲区
   - 滚动文本后绘制，可以覆盖静态文本
   - 确保滚动文本优先显示

2. **滚动文本行清除**
   - 每行滚动文本绘制前先清除该行区域
   - 使用 `fillRect()` 填充黑色矩形
   - 防止与静态文本混合显示

3. **重叠自动处理**
   - 显示新文本时自动检测重叠
   - 清除被覆盖的旧文本
   - 避免视觉混乱

#### 共存示例

```
屏幕布局 (64x32):
┌─────────────────────────────────┐
│ Line 1: 静态标题 [font size=2]  │  ← 静态文本 (y=0-31)
├─────────────────────────────────┤
│ >>> Line 2滚动中 [size=1, left]  │  ← 滚动文本 (y=16-31)
└─────────────────────────────────┘

渲染流程：
1. clearScreen()
   → 清除整个后缓冲区为黑色

2. renderStaticTexts()
   → 设置 font size=2
   → 绘制 "静态标题" 到 y=0
   → 占用区域: y=0~31 (font size=2 高度32)

3. renderScrollingTexts()
   → Line 2: 先清除区域 y=16~31 (黑色矩形)
   → 设置 font size=1
   → 绘制滚动文本到 y=16
   → 文本位置根据 xPosition 动态变化

4. flipDMABuffer()
   → 交换前后缓冲区指针
   → DMA开始显示新帧（瞬间完成，无闪烁）
```

#### 多行滚动文本示例

```
屏幕布局 (64x32):
┌─────────────────────────────────┐
│ >>> Line 1滚动中 [size=1, left]  │  ← 滚动文本1
├─────────────────────────────────┤
│ <<< Line 2滚动中 [size=1, right] │  ← 滚动文本2
└─────────────────────────────────┘

渲染流程：
1. clearScreen()
   → 清除整个后缓冲区

2. renderStaticTexts()
   → 无静态文本，跳过

3. renderScrollingTexts()
   → Line 1:
     ├── 清除区域 y=0~15
     ├── 更新 xPosition (向左移动1像素)
     └── 绘制文本

   → Line 2:
     ├── 清除区域 y=16~31
     ├── 更新 xPosition (向右移动1像素)
     └── 绘制文本

4. flipDMABuffer()
   → 交换缓冲区
```

### 帧时间控制

#### 参数配置

```cpp
// DisplayManager.h
unsigned long lastFrameTime;  // 上一帧的时间戳
unsigned long frameInterval;  // 目标帧间隔（毫秒），默认20ms = 50fps
```

#### 帧率计算

```
帧率 = 1000 / frameInterval

frameInterval = 20ms  →  50fps  (推荐)
frameInterval = 33ms  →  30fps
frameInterval = 16ms  →  62fps
```

#### 实际测量

```
测量方法：
unsigned long frameStart = micros();
renderFrame();
unsigned long frameTime = micros() - frameStart;
Serial.printf("Frame time: %lu us\n", frameTime);

典型值：
- clearScreen(): ~100-200 us
- renderStaticTexts(): ~500-1000 us (取决于文本数量)
- renderScrollingTexts(): ~500-1500 us (取决于滚动行数)
- flipDMABuffer(): ~50-100 us
- 总计: ~1-3 ms (远小于20ms)
```

---

## API 参考

### 初始化和控制

#### init()
初始化显示驱动。

```cpp
displayManager.init();
```

#### loop()
主渲染循环，必须在主程序中持续调用。

```cpp
void loop() {
    displayManager.loop();
}
```

---

### 文本控制

#### setTextSize(int size)
设置文本大小。

**参数：**
- `size`: 字体大小（1=小，2=大）

**影响范围：**
- 静态文本的字体大小
- 新创建的滚动文本的字体大小
- **不影响**已存在的滚动文本

**示例：**
```cpp
displayManager.setTextSize(1);  // 小字体（16像素高）
displayManager.setTextSize(2);  // 大字体（32像素高）
```

---

#### setTextColor(uint16_t color)
设置静态文本颜色。

**参数：**
- `color`: 颜色值（RGB565格式）

**示例：**
```cpp
displayManager.setTextColor(displayManager.redColor);
```

---

#### displayText()

显示文本（静态或滚动）。

**函数签名：**
```cpp
void displayText(const char *textContent, 
               bool isScroll, 
               uint16_t color = 0, 
               int line = -1, 
               bool autoWrap = true,
               int scrollSpeed = 2, 
               ScrollDirection direction = SCROLL_LEFT)
```

**参数说明：**

| 参数 | 类型 | 说明 |
|------|------|------|
| `textContent` | `const char*` | 要显示的文本内容（支持UTF-8中文） |
| `isScroll` | `bool` | 是否滚动文本（true=滚动，false=静态） |
| `color` | `uint16_t` | 文本颜色（0=白色，或使用预定义颜色） |
| `line` | `int` | 行号（见下方详细说明） |
| `autoWrap` | `bool` | 静态文本是否自动换行（默认true） |
| `scrollSpeed` | `int` | 滚动速度（1=慢，2=中，3=快） |
| `direction` | `ScrollDirection` | 滚动方向（SCROLL_LEFT 或 SCROLL_RIGHT） |

#### line 参数详解

**静态文本（isScroll=false）：**
- `line <= 0`（包括 -1）：清屏并全屏显示
- `line > 0`：在指定行显示，不清屏

**滚动文本（isScroll=true）：**
- `line = 1`：Y 坐标 0
- `line = 2`：Y 坐标 16
- `line = 3`：Y 坐标 32（64x64屏幕）
- `line = 4`：Y 坐标 48（64x64屏幕）

**最大行数：** `maxLines = PANEL_RES_Y / 16`

| 屏幕尺寸 | maxLines | 有效行号范围 |
|-----------|----------|--------------|
| 64x32 | 2 | 1-2 |
| 64x64 | 4 | 1-4 |
| 64x128 | 8 | 1-8 |

#### 预定义颜色

```cpp
displayManager.blackColor   // 黑色 (0x0000)
displayManager.whiteColor   // 白色 (0xFFFF)
displayManager.redColor     // 红色
displayManager.greenColor   // 绿色
displayManager.blueColor    // 蓝色
displayManager.yellowColor  // 黄色
displayManager.pinkColor   // 粉色
```

#### 使用示例

```cpp
// 静态文本 - 全屏显示（line=-1，采用非渲染模式）
displayManager.displayText("Hello World", false, 0xFFFF, -1, true);

// 静态文本 - 第1行显示
displayManager.displayText("Hello", false, 0xFFFF, 1, false);

// 滚动文本 - 第1行，小字体，向左滚动，中速
displayManager.setTextSize(1);
displayManager.displayText("Hello", true, displayManager.whiteColor, 1, false, 2, DisplayManager::SCROLL_LEFT);

// 滚动文本 - 第2行，大字体，向右滚动，快速
displayManager.setTextSize(2);
displayManager.displayText("World", true, displayManager.redColor, 2, false, 3, DisplayManager::SCROLL_RIGHT);
```

**注意**：全屏静态文本(line=-1)采用非渲染模式，类似于图片显示，loop() 会跳过渲染以提高效率。此时文本颜色由 payload 的 color 参数指定。

---

### 清除功能

#### clearAll()
清除整个屏幕、停止所有滚动、退出图片显示模式和全屏显示模式。

```cpp
displayManager.clearAll();
```

#### clearArea(x, y, width, height)
清除指定矩形区域。

**参数：**
- `x`: 左上角 X 坐标
- `y`: 左上角 Y 坐标
- `width`: 宽度（像素）
- `height`: 高度（像素）

**示例：**
```cpp
displayManager.clearArea(0, 0, 32, 32);  // 清除左上角 32x32 区域
```

#### clearLine(line)
清除指定行的所有内容（包括静态和滚动文本）。

**参数：**
- `line`: 行号（从1开始）

**示例：**
```cpp
displayManager.clearLine(1);  // 清除第1行
```

#### clearScrollLine(line)
清除指定行的滚动文本。

**参数：**
- `line`: 滚动行号（1-maxLines）

**示例：**
```cpp
displayManager.clearScrollLine(1);  // 清除第1行的滚动文本
```

---

### 亮度控制

#### setBrightness(brightness)
设置屏幕亮度。

**参数：**
- `brightness`: 亮度值（0-255，默认128）

**示例：**
```cpp
displayManager.setBrightness(200);  // 设置较亮
displayManager.setBrightness(50);   // 设置较暗
```

---

### 滚动文本控制

这些函数可以动态修改已存在的滚动文本属性。

#### setLineScrollSpeed(line, speed)
设置指定滚动行的滚动速度。

**参数：**
- `line`: 滚动行号（1-maxLines）
- `speed`: 速度等级（1=慢，2=中，3=快）

**示例：**
```cpp
displayManager.setLineScrollSpeed(1, 3);  // 第1行快速滚动
```

#### setLineColor(line, color)
设置指定滚动行的颜色。

**参数：**
- `line`: 滚动行号（1-maxLines）
- `color`: 颜色值（RGB565格式）

**示例：**
```cpp
displayManager.setLineColor(1, displayManager.redColor);  // 第1行红色
```

#### setLineScrollDirection(line, direction)
设置指定滚动行的滚动方向。

**参数：**
- `line`: 滚动行号（1-maxLines）
- `direction`: 滚动方向（SCROLL_LEFT 或 SCROLL_RIGHT）

**示例：**
```cpp
displayManager.setLineScrollDirection(1, DisplayManager::SCROLL_RIGHT);  // 第1行向右滚动
```

---

### 图片显示

#### displayImage(base64Data, length)
显示 Base64 编码的 RGB565 格式图像。

**参数：**
- `base64Data`: Base64 编码的图像数据字符串
- `length`: 数据长度（字节）

**图像规格：**
- 格式：RGB565（每个像素2字节，小端序）
- 尺寸：必须匹配 PANEL_RES_X × PANEL_RES_Y
- 最大数据：MAX_MQTT_PAYLOAD_SIZE（12800字节）

**示例：**
```cpp
const char* base64Image = "iVBORw0KGgoAAAANSUhEUg...";  // Base64 编码的RGB565图像
displayManager.displayImage(base64Image, strlen(base64Image));
```

**图像大小示例：**

| 屏幕尺寸 | 原始大小 | Base64大小（约） |
|-----------|-----------|------------------|
| 64x32 | 4096字节 | 5461字节 |
| 64x64 | 8192字节 | 10923字节 |
| 64x128 | 16384字节 | 超出限制 |

**注意：**
- 显示图片会自动退出图片模式，清除所有文本内容
- 进入图片显示模式后，loop() 会跳过渲染以保持图片静止
- 进入全屏静态文本模式后，loop() 也会跳过渲染以提高效率
- 调用 displayText() 或 clearAll() 会退出图片模式或全屏显示模式

---

## 使用场景

### 场景 1：单行静态文本

```cpp
displayManager.setTextSize(1);
displayManager.displayText("Hello World", false, 0xFFFF, 1, false);
```

### 场景 2：全屏静态文本

```cpp
displayManager.setTextSize(2);
displayManager.displayText("全屏显示", false, displayManager.redColor, -1, true);
// 全屏静态文本采用非渲染模式，loop() 会跳过渲染以提高效率
```

### 场景 3：单行滚动文本

```cpp
displayManager.setTextSize(1);
displayManager.displayText("滚动文本", true, 0xFFFF, 1, false, 2, DisplayManager::SCROLL_LEFT);
```

### 场景 4：多行不同字体大小的滚动文本

```cpp
// 第1行：小字体，中速向左滚动
displayManager.setTextSize(1);
displayManager.displayText("Hello", true, 0xFFFF, 1, false, 2, DisplayManager::SCROLL_LEFT);

// 第2行：大字体，慢速向右滚动
displayManager.setTextSize(2);
displayManager.displayText("世界", true, displayManager.redColor, 2, false, 1, DisplayManager::SCROLL_RIGHT);
// 两个文本会同时滚动，互不干扰
```

### 场景 5：静态文本 + 滚动文本

```cpp
// 第1行显示静态标题
displayManager.setTextSize(1);
displayManager.displayText("通知:", false, displayManager.yellowColor, 1, false);

// 第2行显示滚动消息
displayManager.displayText("重要公告！", true, 0xFFFF, 2, false, 2, DisplayManager::SCROLL_LEFT);
```

### 场景 6：显示图片

```cpp
const char* base64Image = "...";  // Base64编码的RGB565图像
displayManager.displayImage(base64Image, strlen(base64Image));
// 图片会静止显示，直到调用displayText或clearAll
```

---

## 重要注意事项

### 1. Y 坐标系统

滚动文本的 Y 坐标固定为：`(line - 1) * 16`

| 行号 | Y 坐标 | 说明 |
|-------|---------|------|
| 1 | 0 | 第1行 |
| 2 | 16 | 第2行 |
| 3 | 32 | 第3行（64x64屏幕） |
| 4 | 48 | 第4行（64x64屏幕） |

**字体高度差异：**
- textSize=1: 高度 16 像素，占1行
- textSize=2: 高度 32 像素，占2行

**注意：** 大字号的文本会覆盖后续行的区域，系统会自动清除重叠的文本。

### 2. 自动冲突检测

当显示新文本时，系统会自动检测并清除被覆盖的滚动文本和静态文本。无需手动处理冲突。

### 3. 循环调用 loop()

必须在主循环中调用 `displayManager.loop()`，滚动文本才能正常更新：

```cpp
void loop() {
    displayManager.loop();
    // 其他代码...
}
```

### 4. 内存管理

- 滚动文本和静态文本使用动态内存分配（`malloc`）
- 系统会自动管理内存，释放被覆盖的文本
- 建议不要频繁创建和销毁超长文本

### 5. UTF-8 支持

系统支持 UTF-8 编码，可以正常显示中文、emoji 等字符：
- ASCII字符：1字节
- 中文：3字节
- Emoji：4字节

### 6. 图片显示模式和全屏显示模式

- 进入图片模式或全屏静态文本模式后，loop() 会跳过渲染以保持内容静止
- 显示新文本或调用 clearAll() 会自动退出图片模式或全屏显示模式
- 图片数据必须是 RGB565 格式（小端序）

---

## 配置参数

在 `config.h` 中可以修改以下参数：

```cpp
// 屏幕尺寸
#define PANEL_RES_X 64      // 屏幕宽度
#define PANEL_RES_Y 32      // 屏幕高度
#define PANEL_CHAIN 1       // 级联数量

// 显示配置
#define DEFAULT_BRIGHTNESS 128  // 默认亮度（0-255）

// 滚动速度配置
// 采用帧计数机制，确保滚动更新与帧渲染完全同步
// 渲染频率：固定50fps，每帧20ms
#define SCROLL_OFFSET_LEFT -1        // 向左滚动偏移
#define SCROLL_OFFSET_RIGHT 1        // 向右滚动偏移

// 滚动速度说明：
// - 速度等级1=慢(每3帧48ms移动1像素)
// - 速度等级2=中(每2帧32ms移动1像素)
// - 速度等级3=快(每帧16ms移动1像素)

// MQTT配置
#define MAX_MQTT_PAYLOAD_SIZE 12800  // 最大MQTT载荷（字节）
```

---

## 故障排除

### 问题：滚动文本不动
**解决：** 确保在 `loop()` 中调用了 `displayManager.loop()`

### 问题：文本显示不全
**解决：** 
- 检查文本长度和字体大小
- 考虑使用自动换行或减小字体
- 检查行号是否在有效范围内

### 问题：大字体文本被截断
**解决：** 大字体文本应使用较小的行号（如 line=1 或 line=2），避免超出屏幕

### 问题：多个滚动文本互相覆盖
**解决：** 系统会自动处理覆盖冲突。如需共存，请确保 Y 坐标不重叠

### 问题：图片无法显示
**解决：**
- 检查图片格式是否为 RGB565
- 检查图片尺寸是否匹配屏幕分辨率
- 检查 Base64 编码是否正确
- 检查数据大小是否超过 MAX_MQTT_PAYLOAD_SIZE

### 问题：显示图片后文本不显示
**解决：** 正常现象，图片模式下会跳过渲染。调用 displayText() 会自动退出图片模式。

---

## 总结

DisplayManager 提供了完整的 LED 矩阵显示功能，包括：

1. **多行滚动文本**：每行独立控制速度、方向、颜色、字体大小
2. **静态文本显示**：支持自动换行和全屏显示
3. **图片显示**：支持 Base64 RGB565 图像
4. **自动冲突处理**：智能检测和清除重叠文本
5. **高性能渲染**：50fps 刷新率，DMA 双缓冲

遵循本文档的说明，可以充分发挥 LED 矩阵屏的功能。
