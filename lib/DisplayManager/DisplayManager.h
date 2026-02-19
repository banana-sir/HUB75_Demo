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
        int scrollSpeed;      // 该行的滚动速度（1=慢，2=中，3=快）
        int scrollXMove;      // 该行每次滚动移动的像素数
        int scrollTimeDelay;  // 该行的滚动时间间隔
        ScrollDirection scrollDirection;  // 滚动方向
        uint16_t color;       // 该行的文本颜色
        unsigned long lastUpdateTime;  // 该行上次更新的时间

        ScrollLine() : content(nullptr), xPosition(PANEL_RES_X), yPosition(0),
                       isScrolling(false), isActive(false), scrollSpeed(1),
                       scrollXMove(-1), scrollTimeDelay(30), scrollDirection(SCROLL_LEFT),
                       color(0), lastUpdateTime(0) {}
    };

    ScrollLine *scrollLines;  // 每行的滚动状态数组
    int textSize;              // 文本大小
    int maxLines;              // 最大行数

    void freeAllScrollLines();  // 释放所有滚动行内存
    void calculateScrollSpeedParams(int speed, int& xMove, int& timeDelay, ScrollDirection direction);  // 计算滚动速度参数
    int calculateTextLines(const char *textContent, int startLine);  // 计算文本需要占用的行数

public:
    DisplayManager();
    ~DisplayManager();

    uint16_t blackColor; 
    uint16_t whiteColor;
    uint16_t redColor;
    uint16_t greenColor;
    uint16_t blueColor;
    uint16_t yellowColor;
    uint16_t pinkColor;

    void init();
    void loop();

    void clearAll();
    void clearArea(uint16_t x, uint16_t y, uint16_t width, uint16_t height);
    void clearLine(uint16_t line);
    
    void setTextSize(int size);  // 设置文本大小
    void setTextColor(uint16_t color);  // 设置静态文本颜色
    void setBrightness(uint8_t brightness); // 设置亮度
    // 通用显示文本接口：
    // - textContent: 要显示的文本
    // - isScroll: 是否滚动
    // - color: 文本颜色（为0时使用白色）
    // - line: 滚动时指定行（从1开始）；静态文本为 -1 或 1 表示全屏显示并清屏
    // - autoWrap: 静态文本是否自动换行（默认 true）。当为 true 时会调用清屏/清行并启用自动换行。
    // - scrollSpeed: 滚动速度等级（1=慢，2=中，3=快）
    // - direction: 滚动方向
    void displayText(const char *textContent, bool isScroll, uint16_t color = 0,  int line = -1, bool autoWrap = true,int scrollSpeed = 1, ScrollDirection direction = SCROLL_LEFT);
    void clearScrollLine(int line);  // 清除指定行的滚动状态
    void setLineScrollSpeed(int line, int speed);  // 设置指定行的滚动速度
    void setLineColor(int line, uint16_t color);  // 设置指定行的颜色
    void setLineScrollDirection(int line, ScrollDirection direction);  // 设置指定行的滚动方向
    void displayImage(const char *base64Data, int length);  // 显示base64编码的rgb565图像

};

// 全局实例声明
extern DisplayManager displayManager;

#endif // DISPLAY_MANAGER_H