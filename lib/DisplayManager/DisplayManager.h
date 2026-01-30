#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include "esp_task_wdt.h"
#include "../../include/config.h"

class DisplayManager {
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
        
        ScrollLine() : content(nullptr), xPosition(PANEL_RES_X), yPosition(0), 
                       isScrolling(false), isActive(false) {}
    };

    ScrollLine *scrollLines;  // 每行的滚动状态数组
    int textSize;              // 文本大小
    int maxLines;              // 最大行数
    int scrollTextSpeed;       // 滚动速度（1=慢，2=中，3=快）
    int scrollTextTimeDelay;   // 滚动时间间隔
    int scrollXMove;           // 每次滚动移动的像素数
    unsigned long isAnimationDue;  // 下一次动画更新的时间点

    void freeAllScrollLines();  // 释放所有滚动行内存

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
    void update();

    void clearAll();
    void clearArea(uint16_t x, uint16_t y, uint16_t width, uint16_t height);
    void clearLine(uint16_t line);
    void setTextSize(int size);
    void setTextColor(uint16_t color);
    void setTextScrollSpeed(int speed);
    void setBrightness(uint8_t brightness);
    void displayText(const char *textContent, bool isScroll);                    // 显示文本（单行模式）
    void displayText(const char *textContent, bool isScroll, int line);          // 显示文本（多行模式，指定行号）
    void clearScrollLine(int line);  // 清除指定行的滚动状态

};

// 全局实例声明
extern DisplayManager displayManager;

#endif // DISPLAY_MANAGER_H