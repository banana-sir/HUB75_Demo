# DisplayManager 使用文档

## 概述

DisplayManager 是用于控制 HUB75 LED 矩阵显示屏的管理类，支持静态文本显示、滚动文本显示和图像显示。

## 核心特性

### 1. 多字体大小同时滚动
不同字体大小的滚动文本可以同时存在，互不干扰。每个滚动文本独立管理自己的字体大小、颜色、速度和滚动方向。

### 2. 自动冲突检测
当显示新的文本（静态或滚动）时，会自动检测并清除被新文本覆盖的滚动文本，避免显示冲突。

### 3. 灵活的显示模式
- 支持静态文本和滚动文本
- 支持自动换行（仅静态文本，且无滚动文本时）
- 支持多种滚动速度和方向
- 支持多种预定义颜色

## API 参考

### setTextSize(int size)
设置文本大小。

**参数：**
- `size`: 字体大小（1-2）

**影响范围：**
- 静态文本的字体大小
- 新创建的滚动文本的字体大小
- **不影响**已存在的滚动文本

**示例：**
```cpp
displayManager.setTextSize(1);  // 小字体
displayManager.setTextSize(2);  // 大字体
```

---

### displayText()

显示文本（静态或滚动）。

**函数签名：**
```cpp
void displayText(const char *textContent, 
               bool isScroll, 
               uint16_t color = 0, 
               int line = -1, 
               bool autoWrap = true,
               int scrollSpeed = 1, 
               ScrollDirection direction = SCROLL_LEFT)
```

**参数说明：**

| 参数 | 类型 | 说明 |
|------|------|------|
| `textContent` | `const char*` | 要显示的文本内容 |
| `isScroll` | `bool` | 是否滚动文本（true=滚动，false=静态） |
| `color` | `uint16_t` | 文本颜色（0=白色，或使用预定义颜色） |
| `line` | `int` | 行号（见下方详细说明） |
| `autoWrap` | `bool` | 静态文本是否自动换行（默认true，有滚动文本时强制false） |
| `scrollSpeed` | `int` | 滚动速度（1=慢，2=中，3=快） |
| `direction` | `ScrollDirection` | 滚动方向（SCROLL_LEFT 或 SCROLL_RIGHT） |

#### line 参数详解

**静态文本（isScroll=false）：**
- `line <= 0`（包括 -1）：清屏并全屏显示
- `line > 0`：在指定行显示，不清屏

**滚动文本（isScroll=true）：**
- `line = 1`：Y 坐标 0
- `line = 2`：Y 坐标 16
- `line = 3`：Y 坐标 32
- `line = 4`：Y 坐标 48

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

#### autoWrap 限制

当存在任何滚动文本时，静态文本的 `autoWrap` 会被强制设为 `false`，以避免多行文本覆盖多个滚动行。

**示例：**

```cpp
// 静态文本 - 全屏显示
displayManager.displayText("Hello World", false, 0xFFFF, -1, true);

// 静态文本 - 第1行显示
displayManager.displayText("Hello", false, 0xFFFF, 1, false);

// 滚动文本 - 第1行，小字体，向左滚动
displayManager.setTextSize(1);
displayManager.displayText("Hello", true, displayManager.whiteColor, 1, false, 1, SCROLL_LEFT);

// 滚动文本 - 第2行，大字体，向右滚动
displayManager.setTextSize(2);
displayManager.displayText("World", true, displayManager.redColor, 2, false, 2, SCROLL_RIGHT);
```

---

### clearAll()
清除整个屏幕并停止所有滚动文本。

**示例：**
```cpp
displayManager.clearAll();
```

---

### clearArea(x, y, width, height)
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

---

### clearLine(line)
清除指定行的静态文本区域。

**参数：**
- `line`: 行号（从1开始）

**示例：**
```cpp
displayManager.clearLine(1);  // 清除第1行
```

---

### clearScrollLine(line)
清除指定行的滚动文本。

**参数：**
- `line`: 滚动行号（1-4）

**示例：**
```cpp
displayManager.clearScrollLine(1);  // 清除第1行的滚动文本
```

---

### setTextColor(color)
设置静态文本颜色。

**参数：**
- `color`: 颜色值

**示例：**
```cpp
displayManager.setTextColor(displayManager.redColor);
```

---

### setBrightness(brightness)
设置屏幕亮度。

**参数：**
- `brightness`: 亮度值（0-255，默认128）

**示例：**
```cpp
displayManager.setBrightness(200);  // 设置较亮
displayManager.setBrightness(50);   // 设置较暗
```

---

### 滚动文本控制函数

#### setLineScrollSpeed(line, speed)
设置指定滚动行的滚动速度。

**参数：**
- `line`: 滚动行号（1-4）
- `speed`: 速度等级（1=慢，2=中，3=快）

**示例：**
```cpp
displayManager.setLineScrollSpeed(1, 3);  // 第1行快速滚动
```

#### setLineColor(line, color)
设置指定滚动行的颜色。

**参数：**
- `line`: 滚动行号（1-4）
- `color`: 颜色值

**示例：**
```cpp
displayManager.setLineColor(1, displayManager.redColor);  // 第1行红色
```

#### setLineScrollDirection(line, direction)
设置指定滚动行的滚动方向。

**参数：**
- `line`: 滚动行号（1-4）
- `direction`: 滚动方向（SCROLL_LEFT 或 SCROLL_RIGHT）

**示例：**
```cpp
displayManager.setLineScrollDirection(1, SCROLL_RIGHT);  // 第1行向右滚动
```

---

### displayImage(base64Data, length)
显示 Base64 编码的 RGB565 格式图像。

**参数：**
- `base64Data`: Base64 编码的图像数据
- `length`: 数据长度（字节）

**图像规格：**
- 尺寸：64x64 像素
- 格式：RGB565（每个像素2字节）
- 总大小：8192 字节（编码后约 10923 字节 Base64）

**示例：**
```cpp
const char* base64Image = "...";  // Base64 编码的图像数据
displayManager.displayImage(base64Image, strlen(base64Image));
```

---

## 使用场景

### 场景 1：显示单行静态文本

```cpp
displayManager.setTextSize(1);
displayManager.displayText("Hello World", false, 0xFFFF, 1, false);
```

### 场景 2：显示多行静态文本（自动换行）

```cpp
displayManager.setTextSize(1);
displayManager.displayText("This is a long text that will wrap to multiple lines", 
                         false, 0xFFFF, 1, true);
```

### 场景 3：单行滚动文本

```cpp
displayManager.setTextSize(1);
displayManager.displayText("Scrolling Text", true, 0xFFFF, 1, false, 1, SCROLL_LEFT);
```

### 场景 4：多行不同字体大小的滚动文本

```cpp
// 第1行：小字体，快速向左滚动
displayManager.setTextSize(1);
displayManager.displayText("Hello", true, 0xFFFF, 1, false, 2, SCROLL_LEFT);

// 第2行：大字体，慢速向右滚动
displayManager.setTextSize(2);
displayManager.displayText("World", true, displayManager.redColor, 2, false, 1, SCROLL_RIGHT);
// 两个文本会同时滚动，互不干扰
```

### 场景 5：静态文本 + 滚动文本

```cpp
// 第1行显示静态标题
displayManager.setTextSize(1);
displayManager.displayText("Notice:", false, displayManager.yellowColor, 1, false);

// 第2行显示滚动消息
displayManager.displayText("Important announcement!", true, 0xFFFF, 2, false, 1, SCROLL_LEFT);
```

---

## 重要注意事项

### 1. Y 坐标固定

滚动文本的 Y 坐标固定为：
- line=1: Y=0
- line=2: Y=16
- line=3: Y=32
- line=4: Y=48

**注意：** 不同字体大小的文本高度不同：
- textSize=1: 高度 16 像素
- textSize=2: 高度 32 像素
- textSize=3: 高度 48 像素

因此，大字体的文本可能会覆盖后续行的区域。

### 2. 自动冲突检测

当显示新文本时，系统会自动检测并清除被覆盖的滚动文本。无需手动处理冲突。

### 3. 循环调用 loop()

必须在主循环中调用 `displayManager.loop()`，滚动文本才能正常更新：

```cpp
void loop() {
    displayManager.loop();
    // 其他代码...
}
```

### 4. 内存管理

滚动文本使用动态内存分配（`malloc`）。系统会自动管理内存，但建议不要频繁创建和销毁滚动文本。

### 5. UTF-8 支持

系统支持 UTF-8 编码，可以正常显示中文、emoji 等字符。

---

## 配置参数

在 `config.h` 中可以修改以下参数：

```cpp
// 屏幕尺寸
#define PANEL_RES_X 64      // 屏幕宽度
#define PANEL_RES_Y 64      // 屏幕高度
#define PANEL_CHAIN 1       // 级联数量

// 滚动速度配置
#define SCROLL_TIME_DELAY 36  // 滚动延迟（毫秒）

// 滚动速度偏移
#define SCROLL_OFFSET_LEFT_LOW -1
#define SCROLL_OFFSET_LEFT_MEDIUM -2
#define SCROLL_OFFSET_LEFT_FAST -3
#define SCROLL_OFFSET_RIGHT_LOW 1
#define SCROLL_OFFSET_RIGHT_MEDIUM 2
#define SCROLL_OFFSET_RIGHT_FAST 3
```

---

## 故障排除

### 问题：滚动文本不动
**解决：** 确保在 `loop()` 中调用了 `displayManager.loop()`

### 问题：文本显示不全
**解决：** 检查文本长度和字体大小，考虑使用自动换行或减小字体

### 问题：大字体文本被截断
**解决：** 大字体文本应使用较小的行号（如 line=1 或 line=2），避免超出屏幕

### 问题：多个滚动文本互相覆盖
**解决：** 系统会自动处理覆盖冲突。如需共存，请确保 Y 坐标不重叠
