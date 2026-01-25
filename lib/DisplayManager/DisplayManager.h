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

    int scrollTextTimeDelay;
    int scrollXMove;
    unsigned long isAnimationDue;
    int scrollTextXPosition;
    int scrollTextYPosition;
    int16_t xOne, yOne;
    uint16_t scrollTextWidth, scrollTextHeight;
    int textSize;
    bool isTextWrap;
    bool isScrollText;
    int scrollTextSpeed;
    char *scrollTextContent;
    int currentLine;
    int maxLines;
    bool useMultiLine;

    uint16_t blackColor;
    uint16_t whiteColor;
    uint16_t redColor;
    uint16_t greenColor;
    uint16_t blueColor;
    uint16_t yellowColor;
    uint16_t pinkColor;

    void freeScrollText();

public:
    DisplayManager();
    ~DisplayManager();

    void init();
    void update();

    void clearAll();
    void clearArea(uint16_t x, uint16_t y, uint16_t width, uint16_t height);
    void clearLine(uint16_t line);
    void setTextSize(int size);
    void setMultiLineMode(bool enable);
    void setLine(int line);
    void setTextColor(uint16_t color);
    void setTextScrollSpeed(int speed);
    void displayText(const char *textContent, bool isScroll);
    void displayText(const char *textContent, bool isScroll, int line);

    // Getter for colors if needed
    uint16_t getBlackColor() { return blackColor; }
    uint16_t getWhiteColor() { return whiteColor; }
    uint16_t getRedColor() { return redColor; }
    uint16_t getGreenColor() { return greenColor; }
    uint16_t getBlueColor() { return blueColor; }
    uint16_t getYellowColor() { return yellowColor; }
    uint16_t getPinkColor() { return pinkColor; }
};

// 全局实例声明
extern DisplayManager displayManager;

#endif // DISPLAY_MANAGER_H