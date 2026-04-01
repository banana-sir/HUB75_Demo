#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include "../../include/config.h"


class DisplayManager {
public:
    // 滚动方向枚举
    enum ScrollDirection {
        SCROLL_LEFT = 0,   // 向左滚动
        SCROLL_RIGHT = 1   // 向右滚动
    };

    uint16_t blackColor; 
    uint16_t whiteColor;
    uint16_t redColor;
    uint16_t greenColor;
    uint16_t blueColor;
    uint16_t yellowColor;
    uint16_t pinkColor;
    
private:
    HUB75_I2S_CFG::i2s_pins _pins;
    MatrixPanel_I2S_DMA *dma_display;

    // 滚动文本行状态结构体：存储每行的滚动文本信息
    struct ScrollLine {
        char *content;        // 滚动文本内容
        int xPosition;        // 当前X坐标位置
        int yPosition;        // 行的Y坐标（固定）
        bool isScrolling;     // 是否正在滚动
        bool isActive;        // 该行是否激活
        int updateFrameInterval;  // 更新时间间隔（毫秒）
        unsigned long lastUpdateTime;  // 该行上次更新的时间戳
        ScrollDirection scrollDirection;  // 滚动方向
        uint16_t color;       // 该行的文本颜色
        int textSize;         // 该行的字体大小（独立记录）
        uint16_t cachedTextWidth;  // 缓存的文本宽度，避免重复计算

        ScrollLine() : content(nullptr), xPosition(PANEL_RES_X), yPosition(0),
                       isScrolling(false), isActive(false), updateFrameInterval(35),
                       lastUpdateTime(0), scrollDirection(SCROLL_LEFT), color(0), textSize(1), cachedTextWidth(0) {}
    };

    // 静态文本结构体：存储每行的静态文本信息
    struct StaticText {
        char *content;        // 静态文本内容
        int line;             // 行号（1-maxLines）
        uint16_t color;       // 文本颜色
        bool autoWrap;        // 是否自动换行
        bool isActive;        // 该行是否激活
        int textSize;         // 该行的字体大小

        StaticText() : content(nullptr), line(0), color(0xFFFF), autoWrap(true), isActive(false), textSize(1) {}
    };

    ScrollLine *scrollLines;  // 滚动行数组
    StaticText *staticTexts;  // 静态文本数组
    int textSize;              // 当前字体大小（用于静态文本和新滚动文本）
    int maxLines;              // 最大行数（由PANEL_RES_Y/16计算得出，支持最多4个滚动文本）
    bool isFullScreenDisplay;  // 标记是否为全屏显示模式
    char *fullScreenContent;   // 全屏显示的文本内容
    bool isImageDisplayMode;   // 标记是否为图片显示模式

    // 帧时间控制
    unsigned long lastFrameTime;  // 上一帧的时间戳
    unsigned long frameInterval;  // 目标帧间隔（毫秒）

    void freeAllScrollLines();  // 释放所有滚动行内存
    void freeAllStaticTexts();  // 释放所有静态文本内存
    int calculateScrollInterval(int speed);  // 根据速度等级计算更新间隔（毫秒）
    int calculateTextLines(const char *textContent, int startLine);  // 计算文本需要占用的行数

    // 辅助函数
    bool isDisplayReady() const;  // 检查显示设备是否就绪
    bool isValidScrollIndex(int index) const;  // 检查滚动索引是否有效
    bool isValidStaticIndex(int index) const;  // 检查静态索引是否有效
    char* allocateAndCopyString(const char* source);  // 分配并复制字符串
    uint16_t calculateTextWidth(const char* content, int textSize, int x = 0, int y = 0);  // 计算文本宽度
    bool isOverlap(int y1, int h1, int y2, int h2);  // 检查两个垂直区域是否重叠
    int getStaticTextHeight(int staticIndex);  // 获取静态文本占用的物理高度
    int getScrollTextHeight(int scrollIndex);  // 获取滚动文本占用的物理高度
    void clearScrollLineByIndex(int index);  // 清除指定索引的滚动文本
    void clearStaticLineByIndex(int index);  // 清除指定索引的静态文本
    void clearOverlappingAutoWrapTexts(int yPosition, int height);  // 清除与指定区域重叠的自动换行文本
    void clearOverlappingScrollTexts(int yPosition, int height, int excludeIndex = -1);  // 清除与指定区域重叠的滚动文本
    void clearOverlappingStaticTexts(int yPosition, int height, int excludeIndex = -1);  // 清除与指定区域重叠的静态文本
    void clearLinesRange(int startLine, int lineCount);  // 清除指定行范围内的所有内容
    void clearFullScreenContent();  // 清除全屏显示内容

    // 新增渲染函数
    void updateScrolling();     // 只更新滚动状态（位置等），不绘制
    void renderFrame();         // 完整的帧渲染
    void renderStaticTexts();   // 渲染所有静态文本
    void renderScrollingTexts(); // 渲染所有滚动文本

public:
    DisplayManager();
    ~DisplayManager();

    void init();
    void loop();

    void clearAll();
    void clearArea(uint16_t x, uint16_t y, uint16_t width, uint16_t height);
    void clearLine(uint16_t line);
    
    void setTextSize(int size);  // 设置文本大小（仅影响静态文本和新的滚动文本，不影响已存在的滚动文本）
    void setTextColor(uint16_t color);  // 设置静态文本颜色
    void setBrightness(uint8_t brightness); // 设置亮度
    // 通用显示文本接口：
    // - textContent: 要显示的文本
    // - isScroll: 是否滚动文本（true=滚动，false=静态）
    // - color: 文本颜色（RGB565格式，0=白色）
    // - line: 显示行号（1-maxLines，<=0表示全屏）
    // - autoWrap: 是否自动换行（仅静态文本有效）
    // - scrollSpeed: 滚动速度（1=慢，2=中，3=快，仅滚动文本有效）
    // - direction: 滚动方向（SCROLL_LEFT/SCROLL_RIGHT，仅滚动文本有效）
    // - isScroll: 是否滚动
    // - color: 文本颜色（为0时使用白色）
    // - line: 滚动时指定行（1-maxLines）；静态文本时：line<=0（包括-1）清屏并全屏显示，line>0在指定行显示
    // - autoWrap: 静态文本是否自动换行（默认 true）。注意：如果有滚动文本存在，此参数会被强制设为 false 以避免冲突
    // - scrollSpeed: 滚动速度等级（1=慢，2=中，3=快）
    // - direction: 滚动方向
    //
    // 使用说明：
    // - 每个滚动行记录自己的字体大小，不同字体大小的文本可以同时滚动
    // - 静态文本和滚动文本使用相同的行坐标系：每行固定16像素高
    // - line=1在y=0，line=2在y=16，line=3在y=32，line=4在y=48
    // - 最大行数由PANEL_RES_Y/16决定，例如：64x64屏幕支持4行，64x32屏幕支持2行
    // - 静态文本和滚动文本可以选择相同的行号进行显示
    // - 先 setTextSize(size)，再 displayText(text, true, color, line, false, speed, dir)
    // - 例如：
    //   - setTextSize(1); displayText("Hello", true, 0xFFFF, 1); // 第1行，y=0，字体1，高度16
    //   - setTextSize(2); displayText("World", true, 0xFFFF, 2); // 第2行，y=16，字体2，高度32
    //   - 注意：大字号的文本（如textSize=2）会延伸到后续行区域，可能覆盖其他行的内容
    void displayText(const char *textContent, bool isScroll, uint16_t color = 0,  int line = -1, bool autoWrap = true,int scrollSpeed = 2, ScrollDirection direction = SCROLL_LEFT);
    void clearScrollLine(int line);  // 清除指定行的滚动状态
    void setLineScrollSpeed(int line, int speed);  // 设置指定行的滚动速度
    void setLineColor(int line, uint16_t color);  // 设置指定行的颜色
    void setLineScrollDirection(int line, ScrollDirection direction);  // 设置指定行的滚动方向
    void displayImage(const char *base64Data, int length);  // 显示base64编码的rgb565图像

};

// 全局实例声明
extern DisplayManager displayManager;

#endif // DISPLAY_MANAGER_H