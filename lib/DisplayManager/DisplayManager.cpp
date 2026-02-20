#include "DisplayManager.h"
#include <base64.hpp>
#include "esp_task_wdt.h"


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

void DisplayManager::loop() {
    // 检查 DMA 显示和滚动行数组是否已初始化
    if (dma_display == nullptr || scrollLines == nullptr) {
        return;
    }

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
    if (dma_display == nullptr) return;
    freeAllScrollLines();
    dma_display->clearScreen();
}

void DisplayManager::clearArea(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    // 清除指定区域
    if (dma_display == nullptr) return;
    dma_display->fillRect(x, y, width, height, blackColor);
}

int DisplayManager::calculateTextLines(const char *textContent, int startLine) {
    // 计算文本需要占用的行数（用于自动换行场景）
    if (textContent == nullptr || strlen(textContent) == 0) {
        return 1;
    }

    int charWidth = 8 * textSize;  // 英文字符宽度
    int lineWidth = PANEL_RES_X;   // 屏幕宽度
    int currentLine = 0;
    int currentWidth = 0;

    for (int i = 0; textContent[i] != '\0'; ) {
        // 判断是中文字符还是ASCII字符
        if ((textContent[i] & 0x80) != 0) {
            // 中文字符（UTF-8编码，占3字节），宽度是英文字符的2倍
            currentWidth += charWidth * 2;
            i += 3;
        } else if (textContent[i] == '\n') {
            // 手动换行
            currentLine++;
            currentWidth = 0;
            i++;
        } else {
            // ASCII字符
            currentWidth += charWidth;
            i++;
        }

        // 检查是否需要自动换行
        if (currentWidth > lineWidth && textContent[i-1] != '\n') {
            currentLine++;
            currentWidth = (textContent[i-1] & 0x80) != 0 ? charWidth * 2 : charWidth;
        }
    }

    // 加上最后一行
    if (currentWidth > 0 || currentLine == 0) {
        currentLine++;
    }

    return currentLine;
}

void DisplayManager::clearLine(uint16_t line) {
    // 清除指定行（line从1开始）
    if (dma_display == nullptr) return;
    dma_display->fillRect(0, (line-1) * textSize * 16, PANEL_RES_X, textSize * 16, blackColor);
}

void DisplayManager::setTextSize(int size) {
    if (dma_display == nullptr) return;
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
    if (dma_display == nullptr) return;
    dma_display->setTextColor(color);
}

void DisplayManager::setBrightness(uint8_t brightness) {
    // 设置屏幕亮度
    if (dma_display == nullptr) return;
    dma_display->setBrightness(brightness);

}

void DisplayManager::displayText(const char *textContent, bool isScroll, uint16_t color, int line, bool autoWrap, int scrollSpeed, ScrollDirection direction) {
    // 通用显示函数：既支持静态也支持滚动。
    // 约定：当 line <= 0 时，视为全屏静态显示；否则在指定行进行显示。

    // 静态文本：根据 autoWrap 决定是否启用自动换行（并在需要时清屏/清行）
    if (!isScroll) {
        if (line <= 0) {
            // 全屏显示
            freeAllScrollLines();
            clearAll();
            dma_display->setTextColor((color != 0) ? color : whiteColor);
            dma_display->setCursor(0, 0);
            dma_display->setTextWrap(autoWrap);
            dma_display->printlnUTF8(textContent);
            return;
        } else {
            // 指定行显示：根据 autoWrap 决定清除区域
            int yPosition = (line - 1) * textSize * 16;

            if (autoWrap) {
                // 自动换行时，计算文本需要占用的行数并清除相应区域
                int textLines = calculateTextLines(textContent, line);
                int height = textLines * textSize * 16;
                dma_display->fillRect(0, yPosition, PANEL_RES_X, height, blackColor);
            } else {
                // 不自动换行时，只清除当前行
                clearLine(line);
            }

            dma_display->setTextColor((color != 0) ? color : whiteColor);
            dma_display->setCursor(0, yPosition);
            dma_display->setTextWrap(autoWrap);
            dma_display->printlnUTF8(textContent);
            return;
        }
    }

    // 滚动文本：在指定行显示，使用指定的速度、颜色和方向
    if (line < 1 || line > maxLines) line = 1;

    //delay(30);
    int index = line - 1;

    // 清除该行旧的滚动内容
    if (scrollLines[index].content != nullptr) {
        free(scrollLines[index].content);
        scrollLines[index].content = nullptr;
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

    // 设置该行的颜色（如果未指定颜色，则使用白色）
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

void DisplayManager::displayImage(const char *base64Data, int length) {
    if (base64Data == nullptr || length == 0) {
        Serial.println("displayImage: 数据为空");
        return;
    }

    // 计算base64解码后的数据大小
    unsigned int decodedLen = decode_base64_length((const unsigned char*)base64Data, length);

    Serial.printf("displayImage: base64输入 %d 字节, 预计解码 %d 字节\n", length, decodedLen);

    int imageSize = PANEL_RES_X * PANEL_RES_Y * 2;  // rgb565每个像素2字节
    Serial.printf("displayImage: 期望图像大小 %d 字节 (%dx%d)\n", imageSize, PANEL_RES_X, PANEL_RES_Y);

    if (decodedLen == 0) {
        Serial.println("displayImage: base64数据无效");
        return;
    }

    // 分配内存
    uint8_t *decodedData = (uint8_t *)malloc(decodedLen);
    if (decodedData == nullptr) {
        Serial.println("displayImage: 内存分配失败");
        return;
    }

    // 解码base64数据
    decode_base64((const unsigned char*)base64Data, length, decodedData);
    Serial.printf("displayImage: 解码完成\n");

    // 计算图像尺寸
    int dataSize = min((int)decodedLen, imageSize);
    int pixelCount = dataSize / 2;

    // 清屏并停止所有滚动
    freeAllScrollLines();
    dma_display->clearScreen();

    // 将rgb565数据绘制到屏幕
    for (int i = 0; i < pixelCount; i++) {
        int x = i % PANEL_RES_X;
        int y = i / PANEL_RES_X;

        // 直接使用rgb565数据（大端序）
        uint16_t color = ((uint16_t)decodedData[i * 2] << 8) | decodedData[i * 2 + 1];

        dma_display->drawPixel(x, y, color);

        // 每128个像素喂狗一次，避免看门狗超时
        if (i % 128 == 0) {
            esp_task_wdt_reset();
        }
    }

    Serial.printf("displayImage: 已绘制 %d 像素\n", pixelCount);

    // 刷新屏幕
    dma_display->flipDMABuffer();
    Serial.println("displayImage: 屏幕刷新完成");

    // 释放内存
    free(decodedData);
}