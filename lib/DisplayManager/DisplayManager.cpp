#include "DisplayManager.h"


DisplayManager::DisplayManager() :
    _pins({R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN, A_PIN, B_PIN, C_PIN, D_PIN, E_PIN, LAT_PIN, OE_PIN, CLK_PIN}),
    dma_display(nullptr),
    scrollLines(nullptr),
    textSize(1),
    maxLines(1),
    blackColor(0), whiteColor(0), redColor(0), greenColor(0), blueColor(0), yellowColor(0), pinkColor(0)
{
}

DisplayManager::~DisplayManager() {
    if (dma_display) {
        delete dma_display;
        dma_display = nullptr;
    }
    freeAllScrollLines();
}

void DisplayManager::init() {
    HUB75_I2S_CFG mxconfig(
        PANEL_RES_X,   // 屏幕宽度
        PANEL_RES_Y,   // 屏幕高度
        PANEL_CHAIN,   // 级联数量
        _pins          // 引脚映射
    );

    dma_display = new MatrixPanel_I2S_DMA(mxconfig);
    dma_display->begin();
    dma_display->setBrightness(DEFAULT_BRIGHTNESS);

    // 初始化预定义颜色
    blackColor = dma_display->color565(0, 0, 0);
    whiteColor = dma_display->color565(255, 255, 255);
    redColor = dma_display->color565(255, 0, 0);
    greenColor = dma_display->color565(0, 255, 0);
    blueColor = dma_display->color565(0, 0, 255);
    yellowColor = dma_display->color565(255, 255, 0);
    pinkColor = dma_display->color565(255, 0, 255);

    dma_display->clearScreen();

    setTextSize(1);

    // 初始化滚动行数组
    if (scrollLines == nullptr) {
        scrollLines = new ScrollLine[maxLines];
    }
}

void DisplayManager::update() {
    unsigned long now = millis();

    // 检查是否有任何滚动行需要更新
    bool hasScrollingLines = false;
    for (int i = 0; i < maxLines; i++) {
        if (scrollLines[i].isActive && scrollLines[i].isScrolling && scrollLines[i].content) {
            hasScrollingLines = true;
            break;
        }
    }

    // 如果没有滚动行，直接返回
    if (!hasScrollingLines) return;

    // 刷新DMA缓冲区
    dma_display->flipDMABuffer();

    // 更新所有激活的滚动行，每行独立更新
    for (int i = 0; i < maxLines; i++) {
        if (scrollLines[i].isActive && scrollLines[i].isScrolling && scrollLines[i].content) {
            // 检查该行是否到达更新时间
            if (now > scrollLines[i].lastUpdateTime) {
                int yPosition = scrollLines[i].yPosition;

                // 清除当前行区域
                dma_display->fillRect(0, yPosition, PANEL_RES_X, textSize * 16, blackColor);

                // 更新文本位置
                scrollLines[i].xPosition += scrollLines[i].scrollXMove;

                // 检查文本是否完全移出屏幕，根据滚动方向重置位置
                int16_t xOne, yOne;
                uint16_t textWidth, textHeight;
                dma_display->getTextBounds(scrollLines[i].content, scrollLines[i].xPosition, yPosition,
                                          &xOne, &yOne, &textWidth, &textHeight);

                if (scrollLines[i].scrollDirection == SCROLL_LEFT) {
                    // 向左滚动：文本完全移出左侧时重置到右侧
                    if (scrollLines[i].xPosition + textWidth <= 0) {
                        scrollLines[i].xPosition = PANEL_RES_X;
                    }
                } else {
                    // 向右滚动：文本完全移出右侧时重置到左侧
                    if (scrollLines[i].xPosition >= PANEL_RES_X) {
                        scrollLines[i].xPosition = -textWidth;
                    }
                }

                // 设置该行的颜色并绘制滚动文本
                dma_display->setTextColor(scrollLines[i].color);
                dma_display->setCursor(scrollLines[i].xPosition, yPosition);
                dma_display->printlnUTF8(scrollLines[i].content);

                // 设置下一次该行的更新时间
                scrollLines[i].lastUpdateTime = now + scrollLines[i].scrollTimeDelay;
            }
        }
    }
}

void DisplayManager::freeAllScrollLines() {
    // 释放所有滚动行的内存并重置状态
    if (scrollLines) {
        for (int i = 0; i < maxLines; i++) {
            if (scrollLines[i].content != nullptr) {
                free(scrollLines[i].content);
                scrollLines[i].content = nullptr;
            }
            scrollLines[i].isActive = false;
            scrollLines[i].isScrolling = false;
        }
    }
}

void DisplayManager::clearScrollLine(int line) {
    // 清除指定行的滚动状态并释放内存
    if (line >= 1 && line <= maxLines && scrollLines) {
        int index = line - 1;
        if (scrollLines[index].content != nullptr) {
            free(scrollLines[index].content);
            scrollLines[index].content = nullptr;
        }
        scrollLines[index].isActive = false;
        scrollLines[index].isScrolling = false;
        scrollLines[index].xPosition = PANEL_RES_X;
    }
}

void DisplayManager::calculateScrollSpeedParams(int speed, int& xMove, int& timeDelay, ScrollDirection direction) {
    // 根据滚动速度等级计算对应的像素移动值和时间延迟
    switch (speed) {
        case 1:
            xMove = (direction == SCROLL_LEFT) ? SCROLL_OFFSET_LEFT_LOW : SCROLL_OFFSET_RIGHT_LOW;
            timeDelay = SCROLL_TIME_DELAY;
            break;
        case 2:
            xMove = (direction == SCROLL_LEFT) ? SCROLL_OFFSET_LEFT_MEDIUM : SCROLL_OFFSET_RIGHT_MEDIUM;
            timeDelay = SCROLL_TIME_DELAY;
            break;
        case 3:
            xMove = (direction == SCROLL_LEFT) ? SCROLL_OFFSET_LEFT_FAST : SCROLL_OFFSET_RIGHT_FAST;
            timeDelay = SCROLL_TIME_DELAY;
            break;
        default:
            xMove = (direction == SCROLL_LEFT) ? SCROLL_OFFSET_LEFT_LOW : SCROLL_OFFSET_RIGHT_LOW;
            timeDelay = SCROLL_TIME_DELAY;
            break;
    }
}

void DisplayManager::setLineScrollSpeed(int line, int speed) {
    // 设置指定行的滚动速度，不影响其他行
    if (line >= 1 && line <= maxLines && scrollLines) {
        int index = line - 1;
        if (scrollLines[index].isActive) {
            scrollLines[index].scrollSpeed = speed;

            int xMove, timeDelay;
            calculateScrollSpeedParams(speed, xMove, timeDelay, scrollLines[index].scrollDirection);

            scrollLines[index].scrollXMove = xMove;
            scrollLines[index].scrollTimeDelay = timeDelay;
            scrollLines[index].lastUpdateTime = millis();
        }
    }
}

void DisplayManager::setLineScrollDirection(int line, ScrollDirection direction) {
    // 设置指定行的滚动方向，不影响其他行
    if (line >= 1 && line <= maxLines && scrollLines) {
        int index = line - 1;
        if (scrollLines[index].isActive) {
            scrollLines[index].scrollDirection = direction;

            int xMove, timeDelay;
            calculateScrollSpeedParams(scrollLines[index].scrollSpeed, xMove, timeDelay, direction);

            scrollLines[index].scrollXMove = xMove;
            scrollLines[index].scrollTimeDelay = timeDelay;
            scrollLines[index].lastUpdateTime = millis();

            // 如果切换方向，重置文本位置
            if (direction == SCROLL_LEFT) {
                scrollLines[index].xPosition = PANEL_RES_X;
            } else {
                // 计算文本宽度并设置到左侧
                int16_t xOne, yOne;
                uint16_t textWidth, textHeight;
                if (scrollLines[index].content) {
                    dma_display->getTextBounds(scrollLines[index].content, 0, scrollLines[index].yPosition,
                                              &xOne, &yOne, &textWidth, &textHeight);
                    scrollLines[index].xPosition = -textWidth;
                }
            }
        }
    }
}

void DisplayManager::setLineColor(int line, uint16_t color) {
    // 设置指定行的颜色，不影响其他行
    if (line >= 1 && line <= maxLines && scrollLines) {
        int index = line - 1;
        if (scrollLines[index].isActive) {
            scrollLines[index].color = color;
        }
    }
}

void DisplayManager::clearAll() {
    // 清除整个屏幕
    freeAllScrollLines();
    dma_display->clearScreen();
}

void DisplayManager::clearArea(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    // 清除指定区域
    dma_display->fillRect(x, y, width, height, blackColor);
}

void DisplayManager::clearLine(uint16_t line) {
    // 清除指定行（line从1开始）
    dma_display->fillRect(0, (line-1) * textSize * 16, PANEL_RES_X, textSize * 16, blackColor);
}

void DisplayManager::setTextSize(int size) {
    textSize = size;
    dma_display->setTextSize(textSize);

    // 计算最大行数（字体高度是16 * textSize）
    int newMaxLines = PANEL_RES_Y / (textSize * 16);
    if (newMaxLines < 1) newMaxLines = 1;

    // 如果行数变化，需要重新分配滚动行数组
    if (newMaxLines != maxLines) {
        freeAllScrollLines();
        if (scrollLines != nullptr) {
            delete[] scrollLines;
        }
        maxLines = newMaxLines;
        scrollLines = new ScrollLine[maxLines];
    }
}

void DisplayManager::setTextColor(uint16_t color) {
    // 仅用于设置静态文本颜色
    dma_display->setTextColor(color);
}

void DisplayManager::setBrightness(uint8_t brightness) {
    // 设置屏幕亮度
    dma_display->setBrightness(brightness);

}

void DisplayManager::displayText(const char *textContent, bool isScroll, uint16_t color) {
    // 单行模式：清空屏幕后显示文本
    delay(30);
    freeAllScrollLines();
    clearAll();

    if (isScroll) {
        // 滚动模式：在第一行设置滚动文本，使用默认速度 1（慢速），向左滚动
        dma_display->setTextWrap(false);
        int len = strlen(textContent) + 1;
        if (scrollLines[0].content != nullptr) {
            free(scrollLines[0].content);
        }
        scrollLines[0].content = (char *)malloc(len * sizeof(char));
        if (scrollLines[0].content != nullptr) {
            strcpy(scrollLines[0].content, textContent);
        }

        // 设置默认速度参数（慢速）和方向（向左）
        int xMove, timeDelay;
        calculateScrollSpeedParams(1, xMove, timeDelay, SCROLL_LEFT);
        scrollLines[0].scrollSpeed = 1;
        scrollLines[0].scrollDirection = SCROLL_LEFT;
        scrollLines[0].scrollXMove = xMove;
        scrollLines[0].scrollTimeDelay = timeDelay;
        scrollLines[0].color = whiteColor;
        scrollLines[0].xPosition = PANEL_RES_X;
        scrollLines[0].yPosition = 0;
        scrollLines[0].isScrolling = true;
        scrollLines[0].isActive = true;
        scrollLines[0].lastUpdateTime = millis();
    } else {
        // 静态文本模式：支持自动换行
        dma_display->setTextColor(color);
        dma_display->setCursor(0, 0);
        dma_display->setTextWrap(true);
        dma_display->printlnUTF8(textContent);
    }
}

void DisplayManager::displayText(const char *textContent, bool isScroll, int line, int scrollSpeed, uint16_t color, ScrollDirection direction) {
    if (!isScroll) {
        // 静态文本
        displayText(textContent, false, color);
        return;
    }

    // 滚动文本：在指定行显示，使用指定的速度、颜色和方向
    if (line < 1 || line > maxLines) line = 1;

    delay(30);
    int index = line - 1;

    // 清除该行旧的滚动内容
    if (scrollLines[index].content != nullptr) {
        free(scrollLines[index].content);
    }

    // 清除指定行区域
    int yPosition = (line - 1) * textSize * 16;
    dma_display->fillRect(0, yPosition, PANEL_RES_X, textSize * 16, blackColor);

    dma_display->setTextWrap(false);
    int len = strlen(textContent) + 1;
    scrollLines[index].content = (char *)malloc(len * sizeof(char));
    if (scrollLines[index].content != nullptr) {
        strcpy(scrollLines[index].content, textContent);
    }

    // 设置该行的颜色（如果未指定颜色，则使用当前文本颜色）
    scrollLines[index].color = (color != 0) ? color : whiteColor;
    scrollLines[index].scrollDirection = direction;

    // 设置该行的速度参数
    scrollLines[index].scrollSpeed = scrollSpeed;
    int xMove, timeDelay;
    calculateScrollSpeedParams(scrollSpeed, xMove, timeDelay, direction);

    scrollLines[index].scrollXMove = xMove;
    scrollLines[index].scrollTimeDelay = timeDelay;

    // 根据滚动方向设置初始位置
    if (direction == SCROLL_LEFT) {
        scrollLines[index].xPosition = PANEL_RES_X;
    } else {
        // 向右滚动：计算文本宽度，从左侧开始
        int16_t xOne, yOne;
        uint16_t textWidth, textHeight;
        dma_display->getTextBounds(textContent, 0, yPosition, &xOne, &yOne, &textWidth, &textHeight);
        scrollLines[index].xPosition = -textWidth;
    }

    scrollLines[index].yPosition = yPosition;
    scrollLines[index].isScrolling = true;
    scrollLines[index].isActive = true;
    scrollLines[index].lastUpdateTime = millis();
}