#include "DisplayManager.h"
#include <base64.hpp>

DisplayManager::DisplayManager() :
    _pins({R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN, A_PIN, B_PIN, C_PIN, D_PIN, E_PIN, LAT_PIN, OE_PIN, CLK_PIN}),
    dma_display(nullptr),
    scrollLines(nullptr),
    staticTexts(nullptr),
    textSize(1),
    maxLines(PANEL_RES_Y / 16),
    blackColor(0), whiteColor(0), redColor(0), greenColor(0), blueColor(0), yellowColor(0), pinkColor(0),
    isFullScreenDisplay(false),
    fullScreenContent(nullptr),
    fullScreenColor(0),
    isImageDisplayMode(false),
    hasContentToDisplay(false),
    lastFrameTime(0),
    frameInterval(16)  // 16ms ≈ 60fps，更流畅的显示效果
{
}

DisplayManager::~DisplayManager() {
    if (dma_display) {
        delete dma_display;
        dma_display = nullptr;
    }
    freeAllScrollLines();
    freeAllStaticTexts();
    if (fullScreenContent) {
        free(fullScreenContent);
        fullScreenContent = nullptr;
    }
}

void DisplayManager::init() {
    HUB75_I2S_CFG mxconfig(
        PANEL_RES_X,   // 屏幕宽度
        PANEL_RES_Y,   // 屏幕高度
        PANEL_CHAIN,   // 级联数量
        _pins          // 引脚映射
    );

    // 启用DMA双缓冲 - 稳定渲染的关键
    mxconfig.double_buff = true;

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

    // 初始化静态文本数组
    if (staticTexts == nullptr) {
        staticTexts = new StaticText[maxLines];
    }
}

void DisplayManager::loop() {
    // 如果是图片显示模式或全屏静态文本模式或没有内容需要显示，跳过渲染循环（节省资源）
    if (isImageDisplayMode || isFullScreenDisplay || !hasContentToDisplay) {
        return;
    }

    // 标准帧渲染流程：更新 -> 渲染 -> 翻转缓冲区
    unsigned long now = millis();

    // 只在达到目标帧间隔时才进行更新和渲染（保持同步）
    if (now - lastFrameTime >= frameInterval) {
        updateScrolling();  // 先更新滚动位置
        renderFrame();      // 再渲染并flip
        lastFrameTime = now;
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
            scrollLines[i].cachedTextWidth = 0;  // 重置缓存的文本宽度
            scrollLines[i].frameCounter = 0;  // 重置帧计数器
            scrollLines[i].xPosition = PANEL_RES_X;  // 重置X位置
        }
    }
}

void DisplayManager::freeAllStaticTexts() {
    // 释放所有静态文本的内存并重置状态
    if (staticTexts) {
        for (int i = 0; i < maxLines; i++) {
            if (staticTexts[i].content != nullptr) {
                free(staticTexts[i].content);
                staticTexts[i].content = nullptr;
            }
            staticTexts[i].isActive = false;
        }
    }
}

void DisplayManager::updateContentDisplayState() {
    // 检查是否有任何内容需要显示
    bool hasScrolling = false;
    bool hasStatic = false;
    bool hasFullScreen = false;

    // 检查滚动文本
    if (scrollLines) {
        for (int i = 0; i < maxLines; i++) {
            if (scrollLines[i].isActive && scrollLines[i].content) {
                hasScrolling = true;
                break;
            }
        }
    }

    // 检查静态文本
    if (!hasScrolling && staticTexts) {
        for (int i = 0; i < maxLines; i++) {
            if (staticTexts[i].isActive && staticTexts[i].content) {
                hasStatic = true;
                break;
            }
        }
    }

    // 检查全屏显示
    hasFullScreen = isFullScreenDisplay && (fullScreenContent != nullptr);

    // 更新状态
    bool newState = hasScrolling || hasStatic || hasFullScreen;
    if (newState != hasContentToDisplay) {
        hasContentToDisplay = newState;
        DEBUG_LOG("DisplayManager: 内容显示状态 -> %s\n", newState ? "有内容" : "无内容");
    }
}

void DisplayManager::updateScrolling() {
    // 只更新滚动状态（位置等），不进行任何绘制
    if (!isDisplayReady() || scrollLines == nullptr) {
        return;
    }

    // 更新所有激活的滚动行，每行独立更新
    for (int i = 0; i < maxLines; i++) {
        if (scrollLines[i].isActive && scrollLines[i].isScrolling && scrollLines[i].content) {
            // 增加帧计数器
            scrollLines[i].frameCounter++;

            // 当达到更新间隔帧数时，更新位置
            if (scrollLines[i].frameCounter >= scrollLines[i].updateFrameInterval) {
                scrollLines[i].frameCounter = 0;  // 重置计数器

                // 更新文本位置（根据方向移动1像素）
                if (scrollLines[i].scrollDirection == SCROLL_LEFT) {
                    scrollLines[i].xPosition += SCROLL_OFFSET_LEFT;  // 直接加上偏移值（-1，所以是减）
                } else {
                    scrollLines[i].xPosition += SCROLL_OFFSET_RIGHT;  // 直接加上偏移值（+1，所以是加）
                }

                // 检查文本是否完全移出屏幕，根据滚动方向重置位置
                // 使用缓存的文本宽度，避免每帧调用getTextBounds()
                if (scrollLines[i].scrollDirection == SCROLL_LEFT) {
                    // 向左滚动：文本完全移出左侧时重置到右侧
                    if (scrollLines[i].xPosition + scrollLines[i].cachedTextWidth <= 0) {
                        scrollLines[i].xPosition = PANEL_RES_X;
                    }
                } else {
                    // 向右滚动：文本完全移出右侧时重置到左侧
                    if (scrollLines[i].xPosition >= PANEL_RES_X) {
                        scrollLines[i].xPosition = -scrollLines[i].cachedTextWidth;
                    }
                }
            }
        }
    }

    // 恢复全局字体大小
    dma_display->setTextSize(textSize);
}

void DisplayManager::renderFrame() {
    // 完整的帧渲染：清空缓冲区 -> 绘制静态文本 -> 绘制滚动文本 -> 翻转缓冲区
    // 注意：全屏静态文本和图片模式不会调用此函数
    if (!isDisplayReady()) {
        return;
    }

    // 清除后缓冲区
    dma_display->clearScreen();

    // 绘制静态文本和滚动文本（全屏静态文本不会走到这里）
    renderStaticTexts();
    renderScrollingTexts();

    // 翻转DMA缓冲区
    // 注：flipDMABuffer() 会在硬件准备好时进行缓冲区交换，确保同步
    dma_display->flipDMABuffer();
}

void DisplayManager::renderStaticTexts() {
    // 遍历所有静态文本，绘制激活的静态文本
    if (!isDisplayReady() || staticTexts == nullptr) {
        return;
    }

    for (int i = 0; i < maxLines; i++) {
        if (staticTexts[i].isActive && staticTexts[i].content) {
            int yPosition = (staticTexts[i].line - 1) * 16;

            dma_display->setTextSize(staticTexts[i].textSize);
            dma_display->setTextColor(staticTexts[i].color);
            dma_display->setCursor(0, yPosition);
            dma_display->setTextWrap(staticTexts[i].autoWrap);
            dma_display->printlnUTF8(staticTexts[i].content);
        }
    }

    // 恢复全局设置（确保不影响后续滚动文本渲染）
    dma_display->setTextWrap(false);
    dma_display->setTextSize(textSize);
}

void DisplayManager::renderScrollingTexts() {
    // 遍历所有滚动文本行，绘制激活的滚动文本
    if (!isDisplayReady() || scrollLines == nullptr) {
        return;
    }

    // 确保textWrap关闭（很重要！防止滚动文本被意外换行）
    dma_display->setTextWrap(false);

    for (int i = 0; i < maxLines; i++) {
        if (scrollLines[i].isActive && scrollLines[i].isScrolling && scrollLines[i].content) {
            // 如果文本完全超出屏幕，则跳过绘制
            int xPos = scrollLines[i].xPosition;
            int textWidth = scrollLines[i].cachedTextWidth;
            if (xPos >= PANEL_RES_X || xPos + textWidth <= 0) {
                continue;
            }

            // 先清除本行显示区域，避免旧静态文本或重叠内容干扰
            int lineHeight = getScrollTextHeight(i);
            if (lineHeight > 0) {
                dma_display->fillRect(0, scrollLines[i].yPosition, PANEL_RES_X, lineHeight, blackColor);
            }

            // 绘制滚动文本
            dma_display->setTextSize(scrollLines[i].textSize);
            dma_display->setTextColor(scrollLines[i].color);
            dma_display->setTextWrap(false);
            dma_display->setCursor(scrollLines[i].xPosition, scrollLines[i].yPosition);
            dma_display->printUTF8(scrollLines[i].content);
        }
    }

    // 恢复全局字体大小
    dma_display->setTextSize(textSize);
}

void DisplayManager::clearScrollLine(int line) {
    // 清除指定行的滚动状态并释放内存（不影响该行的静态文本）
    if (line >= 1 && line <= maxLines) {
        int index = line - 1;
        clearScrollLineByIndex(index);
    }
}

void DisplayManager::setLineScrollSpeed(int line, int speed) {
    // 设置指定行的滚动速度，不影响其他行
    if (line >= 1 && line <= maxLines && scrollLines) {
        int index = line - 1;
        if (scrollLines[index].isActive) {
            scrollLines[index].updateFrameInterval = calculateScrollInterval(speed);
        }
    }
}

void DisplayManager::setLineScrollDirection(int line, ScrollDirection direction) {
    // 设置指定行的滚动方向，不影响其他行
    if (line >= 1 && line <= maxLines && scrollLines) {
        int index = line - 1;
        if (scrollLines[index].isActive) {
            scrollLines[index].scrollDirection = direction;

            // 如果切换方向，重置文本位置
            if (direction == SCROLL_LEFT) {
                scrollLines[index].xPosition = PANEL_RES_X;
            } else {
                // 使用缓存的文本宽度并设置到左侧
                if (scrollLines[index].content) {
                    scrollLines[index].xPosition = -scrollLines[index].cachedTextWidth;
                }
            }
        }
    }
}

void DisplayManager::setLineColor(int line, uint16_t color) {
    // 设置指定行的颜色，不影响其他行
    if (line >= 1 && line <= maxLines && scrollLines) {
        int index = line - 1;
        if (isValidScrollIndex(index) && scrollLines[index].isActive) {
            scrollLines[index].color = color;
        }
    }
}

void DisplayManager::clearAll() {
    // 清除整个屏幕以及所有缓存的文本
    if (!isDisplayReady()) return;

    // 清除全屏模式
    clearFullScreenContent();

    // 释放所有滚动和静态文本
    freeAllScrollLines();
    freeAllStaticTexts();

    // 清除图片显示模式
    isImageDisplayMode = false;

    // 先清屏（在更新状态之前，确保屏幕被清除）
    dma_display->clearScreen();
    dma_display->flipDMABuffer();

    // 更新内容显示状态（清屏后会跳过渲染）
    updateContentDisplayState();
}

void DisplayManager::clearArea(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    // 清除指定区域
    if (dma_display == nullptr) return;
    dma_display->fillRect(x, y, width, height, blackColor);
}

int DisplayManager::calculateTextLines(const char *textContent, int startLine, int textSizeParam) {
    // 计算文本需要占用的物理行数（用于自动换行场景）
    // 物理行数 = 文本行数 × textSizeParam
    if (textContent == nullptr || strlen(textContent) == 0) {
        return textSizeParam;  // 即使空文本也占用 textSizeParam 个物理行
    }

    int charWidth = 8 * textSizeParam;  // 英文字符宽度（基于传入的字体大小）
    int lineWidth = PANEL_RES_X;   // 屏幕宽度
    int textLines = 0;  // 文本行数（不是物理行数）
    int currentWidth = 0;

    for (int i = 0; textContent[i] != '\0'; ) {
        // 判断是中文字符还是ASCII字符
        if ((textContent[i] & 0x80) != 0) {
            // 中文字符（UTF-8编码，占3字节），宽度是英文字符的2倍
            currentWidth += charWidth * 2;
            i += 3;
        } else if (textContent[i] == '\n') {
            // 手动换行
            textLines++;
            currentWidth = 0;
            i++;
        } else {
            // ASCII字符
            currentWidth += charWidth;
            i++;
        }

        // 检查是否需要自动换行
        if (currentWidth > lineWidth && textContent[i-1] != '\n') {
            textLines++;
            currentWidth = (textContent[i-1] & 0x80) != 0 ? charWidth * 2 : charWidth;
        }
    }

    // 加上最后一行
    if (currentWidth > 0 || textLines == 0) {
        textLines++;
    }

    // 计算实际占用的物理行数（考虑字体大小）
    // 例如：textSizeParam=2时，1行文本占用2个物理行（32像素高）
    int physicalLines = textLines * textSizeParam;

    return physicalLines;
}

int DisplayManager::calculateScrollInterval(int speed) {
    // 根据速度等级计算更新间隔（帧数）
    // 使用帧计数器实现，与渲染帧同步，更平滑
    // 基于60fps（每帧16ms）
    // 1=慢（每3帧移动1像素），2=中（每2帧移动1像素），3=快（每帧移动1像素）
    switch (speed) {
        case 1:
            return 3;  // 慢速：每3帧（48ms）移动1像素
        case 2:
            return 2;  // 中速：每2帧（32ms）移动1像素
        case 3:
            return 1;  // 快速：每帧（16ms）移动1像素
        default:
            return 2;  // 默认中速
    }
}


// ============================================================================
// 辅助函数实现
// ============================================================================

bool DisplayManager::isDisplayReady() const {
    // 检查显示设备是否就绪
    return dma_display != nullptr;
}

bool DisplayManager::isValidScrollIndex(int index) const {
    // 检查滚动索引是否有效
    return index >= 0 && index < maxLines && scrollLines != nullptr;
}

bool DisplayManager::isValidStaticIndex(int index) const {
    // 检查静态索引是否有效
    return index >= 0 && index < maxLines && staticTexts != nullptr;
}

char* DisplayManager::allocateAndCopyString(const char* source) {
    // 分配内存并复制字符串
    if (!source) return nullptr;
    int len = strlen(source) + 1;
    char* dest = (char *)malloc(len * sizeof(char));
    if (dest) {
        strcpy(dest, source);
    }
    return dest;
}

uint16_t DisplayManager::calculateTextWidth(const char* content, int textSize, int x, int y) {
    // 计算文本宽度
    int16_t xOne, yOne;
    uint16_t textWidth, textHeight;
    dma_display->setTextSize(textSize);
    dma_display->getTextBounds(content, x, y, &xOne, &yOne, &textWidth, &textHeight);
    return textWidth;
}

bool DisplayManager::isOverlap(int y1, int h1, int y2, int h2) {
    // 检查两个垂直区域是否重叠
    return !(y1 + h1 <= y2 || y1 >= y2 + h2);
}

void DisplayManager::clearFullScreenContent() {
    // 清除全屏显示内容
    if (fullScreenContent != nullptr) {
        free(fullScreenContent);
        fullScreenContent = nullptr;
    }
    isFullScreenDisplay = false;

    // 更新内容显示状态（仅当独立调用时）
    // 注意：如果是在 clearAll() 或 displayText() 中调用，会由调用者更新状态
}

int DisplayManager::getStaticTextHeight(int staticIndex) {
    // 获取静态文本占用的物理高度（像素）
    if (!isValidStaticIndex(staticIndex) || !staticTexts[staticIndex].isActive) {
        return 0;
    }

    int staticHeight = staticTexts[staticIndex].textSize * 16;
    if (staticTexts[staticIndex].autoWrap && staticTexts[staticIndex].content) {
        int staticLines = calculateTextLines(staticTexts[staticIndex].content, staticTexts[staticIndex].line, staticTexts[staticIndex].textSize);
        staticHeight = staticLines * 16;
    }

    return staticHeight;
}

int DisplayManager::getScrollTextHeight(int scrollIndex) {
    // 获取滚动文本占用的物理高度（像素）
    if (!isValidScrollIndex(scrollIndex) || !scrollLines[scrollIndex].isActive) {
        return 0;
    }
    return scrollLines[scrollIndex].textSize * 16;
}

void DisplayManager::clearScrollLineByIndex(int index) {
    // 清除指定索引的滚动文本
    if (!isValidScrollIndex(index)) return;

    if (scrollLines[index].content != nullptr) {
        free(scrollLines[index].content);
        scrollLines[index].content = nullptr;
    }
    scrollLines[index].isActive = false;
    scrollLines[index].isScrolling = false;
    scrollLines[index].cachedTextWidth = 0;
    scrollLines[index].frameCounter = 0;  // 重置帧计数器
    scrollLines[index].xPosition = PANEL_RES_X;  // 重置X位置

    // 更新内容显示状态
    updateContentDisplayState();
}

void DisplayManager::clearStaticLineByIndex(int index) {
    // 清除指定索引的静态文本
    if (!isValidStaticIndex(index)) return;

    DEBUG_LOG("clearStaticLineByIndex: 清除静态文本[index=%d], line=%d, isActive=%d\n",
             index, staticTexts[index].line, staticTexts[index].isActive);

    if (staticTexts[index].content != nullptr) {
        free(staticTexts[index].content);
        staticTexts[index].content = nullptr;
    }
    staticTexts[index].isActive = false;

    // 更新内容显示状态
    updateContentDisplayState();
}

void DisplayManager::clearOverlappingAutoWrapTexts(int yPosition, int height) {
    // 清除与指定区域重叠的自动换行静态文本
    DEBUG_LOG("clearOverlappingAutoWrapTexts: 开始检查, 目标区域(y=%d, h=%d)\n", yPosition, height);

    for (int i = 0; i < maxLines; i++) {
        if (staticTexts[i].isActive && staticTexts[i].autoWrap && staticTexts[i].content) {
            int staticY = (staticTexts[i].line - 1) * 16;
            int staticHeight = getStaticTextHeight(i);

            bool overlap = isOverlap(yPosition, height, staticY, staticHeight);
            DEBUG_LOG("clearOverlappingAutoWrapTexts: 检查自动换行文本[%d] - line=%d, autoWrap=%d, staticY=%d, staticHeight=%d, 目标(y=%d,h=%d), overlap=%d\n",
                     i, staticTexts[i].line, staticTexts[i].autoWrap, staticY, staticHeight, yPosition, height, overlap);

            if (overlap) {
                DEBUG_LOG("clearOverlappingAutoWrapTexts: 清除重叠的自动换行文本[%d]\n", i);
                clearStaticLineByIndex(i);
            }
        }
    }

    DEBUG_LOG("clearOverlappingAutoWrapTexts: 检查完成\n");
}

void DisplayManager::clearOverlappingScrollTexts(int yPosition, int height, int excludeIndex) {
    // 清除与指定区域重叠的滚动文本
    for (int i = 0; i < maxLines; i++) {
        if (i == excludeIndex) continue;

        if (scrollLines[i].isActive && scrollLines[i].isScrolling && scrollLines[i].content) {
            if (isOverlap(yPosition, height, scrollLines[i].yPosition, getScrollTextHeight(i))) {
                clearScrollLineByIndex(i);
            }
        }
    }
}

void DisplayManager::clearOverlappingStaticTexts(int yPosition, int height, int excludeIndex) {
    // 清除与指定区域重叠的静态文本
    DEBUG_LOG("clearOverlappingStaticTexts: 开始检查, 目标区域(y=%d, h=%d), excludeIndex=%d\n",
             yPosition, height, excludeIndex);

    for (int i = 0; i < maxLines; i++) {
        if (i == excludeIndex) continue;

        if (staticTexts[i].isActive && staticTexts[i].content) {
            int staticY = (staticTexts[i].line - 1) * 16;
            int staticHeight = getStaticTextHeight(i);

            bool overlap = isOverlap(yPosition, height, staticY, staticHeight);
            DEBUG_LOG("clearOverlappingStaticTexts: 检查静态文本[%d] - line=%d, staticY=%d, staticHeight=%d, 目标(y=%d,h=%d), overlap=%d\n",
                     i, staticTexts[i].line, staticY, staticHeight, yPosition, height, overlap);

            if (overlap) {
                DEBUG_LOG("clearOverlappingStaticTexts: 清除重叠的静态文本[%d]\n", i);
                clearStaticLineByIndex(i);
            }
        }
    }

    DEBUG_LOG("clearOverlappingStaticTexts: 检查完成\n");
}

void DisplayManager::clearLinesRange(int startLine, int lineCount) {
    // 清除指定行范围内的所有内容
    for (int i = 0; i < lineCount && (startLine + i) <= maxLines; i++) {
        int index = startLine + i - 1;
        clearScrollLineByIndex(index);
        clearStaticLineByIndex(index);
    }
}


void DisplayManager::clearLine(uint16_t line) {
    // 清除指定行的所有内容（包括滚动和静态文本）
    if (!isDisplayReady()) return;

    if (line >= 1 && line <= maxLines) {
        int index = line - 1;
        clearScrollLineByIndex(index);
        clearStaticLineByIndex(index);
    }
}

void DisplayManager::setTextSize(int size) {
    // 设置全局字体大小，用于新创建的文本
    // 已存在的文本保持自己的字体大小不变
    if (!isDisplayReady()) return;
    textSize = size;
    dma_display->setTextSize(textSize);
}

void DisplayManager::setTextColor(uint16_t color) {
    // 仅用于设置静态文本颜色
    if (!isDisplayReady()) return;
    dma_display->setTextColor(color);
}

void DisplayManager::setBrightness(uint8_t brightness) {
    // 设置屏幕亮度
    if (!isDisplayReady()) return;
    dma_display->setBrightness(brightness);

}

void DisplayManager::displayText(const char *textContent, bool isScroll, uint16_t color, int line, bool autoWrap, int scrollSpeed, ScrollDirection direction) {
    unsigned long startTime = millis();
    DEBUG_LOG("displayText: 开始处理, 时间: %lu ms\n", startTime);
    DEBUG_LOG("displayText: 文本='%s', 滚动=%d, 颜色=%d, 行号=%d, 自动换行=%d, 速度=%d, 方向=%d\n",
             textContent, isScroll, color, line, autoWrap, scrollSpeed, direction);

    // 通用显示函数：既支持静态也支持滚动。
    // 约定：当 line <= 0 时，视为全屏静态显示；否则在指定行进行显示。

    // 退出图片显示模式，恢复正常文本渲染
    if (isImageDisplayMode) {
        isImageDisplayMode = false;
        DEBUG_LOG("displayText: 退出图片显示模式，开始显示文本\n");
    }

    if (!isScroll) {
        DEBUG_LOG("displayText: 静态文本模式\n");
        // 静态文本模式：只保存数据，不直接绘制
        if (line <= 0) {
            // 全屏显示模式
            DEBUG_LOG("displayText: 全屏显示模式\n");
            clearFullScreenContent();
            freeAllScrollLines();
            freeAllStaticTexts();

            fullScreenContent = allocateAndCopyString(textContent);
            if (fullScreenContent == nullptr) {
                DEBUG_LOG("displayText: 内存分配失败\n");
                return;
            }
            fullScreenColor = (color != 0) ? color : whiteColor;
            isFullScreenDisplay = true;

            // 更新内容显示状态（确保渲染循环正常工作）
            updateContentDisplayState();

            // 强制立即渲染一帧（清屏并显示全屏文本）
            dma_display->clearScreen();
            dma_display->setTextColor(fullScreenColor);
            dma_display->setCursor(0, 0);
            dma_display->setTextWrap(true);
            dma_display->printlnUTF8(fullScreenContent);
            dma_display->flipDMABuffer();

            // 全屏静态文本不需要持续渲染，清空内容显示状态标记
            updateContentDisplayState();

            DEBUG_LOG("displayText: 全屏文本已保存, 耗时: %lu ms\n", millis() - startTime);
        } else {
            // 指定行显示模式
            DEBUG_LOG("displayText: 指定行显示模式, line=%d\n", line);
            clearFullScreenContent();

            if (line >= 1 && line <= maxLines && staticTexts != nullptr) {
                int index = line - 1;

                // 计算新文本实际占用的物理行数
                int requiredLines = textSize;
                if (autoWrap) {
                    requiredLines = calculateTextLines(textContent, line);
                }

                // 计算新文本显示区域
                int newYPosition = (line - 1) * 16;
                int newHeight = requiredLines * 16;

                // 清除重叠的文本
                clearOverlappingAutoWrapTexts(newYPosition, newHeight);
                clearStaticLineByIndex(index);
                clearLinesRange(line, requiredLines);

                // 保存静态文本信息
                staticTexts[index].content = allocateAndCopyString(textContent);

                // 如果内存分配失败，直接返回
                if (staticTexts[index].content == nullptr) {
                    DEBUG_LOG("displayText: 内存分配失败\n");
                    return;
                }

                staticTexts[index].line = line;
                staticTexts[index].color = (color != 0) ? color : whiteColor;
                staticTexts[index].autoWrap = autoWrap;
                staticTexts[index].isActive = true;
                staticTexts[index].textSize = textSize;

                // 更新内容显示状态
                updateContentDisplayState();

                DEBUG_LOG("displayText: 静态文本已保存到行 %d, 耗时: %lu ms\n", line, millis() - startTime);
            }
        }

        // 更新内容显示状态
        updateContentDisplayState();

        DEBUG_LOG("displayText: 静态文本处理完成, 总耗时: %lu ms\n", millis() - startTime);
        return;
    }

    // 滚动文本模式：保存滚动文本信息（保持原有逻辑，不直接绘制）
    DEBUG_LOG("displayText: 滚动文本模式, line=%d\n", line);
    clearFullScreenContent();

    if (line < 1 || line > maxLines) line = 1;
    int index = line - 1;

    // 清除该行旧的滚动内容
    clearScrollLineByIndex(index);

    // 计算Y坐标（行号对应的固定Y坐标）
    int yPosition = (line - 1) * 16;

    // 先设置该行的字体大小（使用当前的全局textSize）
    scrollLines[index].textSize = textSize;

    // 使用该行的字体大小计算实际高度
    int lineHeight = scrollLines[index].textSize * 16;

    DEBUG_LOG("displayText: 滚动文本区域 - yPosition=%d, lineHeight=%d, line=%d, textSize=%d\n",
             yPosition, lineHeight, line, scrollLines[index].textSize);

    // 清除重叠的文本
    clearOverlappingAutoWrapTexts(yPosition, lineHeight);
    clearOverlappingScrollTexts(yPosition, lineHeight, index);
    clearStaticLineByIndex(index);
    clearOverlappingStaticTexts(yPosition, lineHeight, index);

    // 设置滚动文本
    dma_display->setTextWrap(false);
    scrollLines[index].content = allocateAndCopyString(textContent);

    // 如果内存分配失败，直接返回
    if (scrollLines[index].content == nullptr) {
        return;
    }

    // 设置该行的颜色（如果未指定颜色，则使用白色）
    scrollLines[index].color = (color != 0) ? color : whiteColor;
    scrollLines[index].scrollDirection = direction;

    // 设置该行的更新间隔
    scrollLines[index].updateFrameInterval = calculateScrollInterval(scrollSpeed);

    // 计算并缓存文本宽度
    scrollLines[index].cachedTextWidth = calculateTextWidth(textContent, scrollLines[index].textSize, 0, yPosition);

    // 根据滚动方向设置初始位置
    if (direction == SCROLL_LEFT) {
        scrollLines[index].xPosition = PANEL_RES_X;
    } else {
        // 向右滚动：使用缓存的文本宽度，从左侧开始
        scrollLines[index].xPosition = -scrollLines[index].cachedTextWidth;
    }

    scrollLines[index].yPosition = yPosition;
    scrollLines[index].isScrolling = true;
    scrollLines[index].isActive = true;

    // 更新内容显示状态
    updateContentDisplayState();

    DEBUG_LOG("displayText: 滚动文本已保存, 总耗时: %lu ms\n", millis() - startTime);
}

void DisplayManager::displayImage(const char *base64Data, int length) {
    unsigned long startTime = millis();
    DEBUG_LOG("displayImage: 开始处理图片, 时间: %lu ms\n", startTime);

    if (base64Data == nullptr || length == 0) {
        DEBUG_LOG("displayImage: 数据为空\n");
        return;
    }

    // 计算base64解码后的数据大小
    unsigned int decodedLen = decode_base64_length((const unsigned char*)base64Data, length);

    DEBUG_LOG("displayImage: base64输入 %d 字节, 预计解码 %d 字节\n", length, decodedLen);

    int imageSize = PANEL_RES_X * PANEL_RES_Y * 2;  // rgb565每个像素2字节
    DEBUG_LOG("displayImage: 期望图像大小 %d 字节 (%dx%d)\n", imageSize, PANEL_RES_X, PANEL_RES_Y);

    if (decodedLen == 0) {
        DEBUG_LOG("displayImage: base64数据无效\n");
        return;
    }

    // 分配内存
    DEBUG_LOG("displayImage: 分配内存...\n");
    uint8_t *decodedData = (uint8_t *)malloc(decodedLen);
    if (decodedData == nullptr) {
        DEBUG_LOG("displayImage: 内存分配失败, 需要 %d 字节\n", decodedLen);
        return;
    }
    DEBUG_LOG("displayImage: 内存分配成功, 耗时: %lu ms\n", millis() - startTime);

    // 解码base64数据
    DEBUG_LOG("displayImage: 开始解码base64...\n");
    decode_base64((const unsigned char*)base64Data, length, decodedData);
    DEBUG_LOG("displayImage: 解码完成, 耗时: %lu ms\n", millis() - startTime);

    // 计算图像尺寸
    int dataSize = min((int)decodedLen, imageSize);
    int pixelCount = dataSize / 2;
    DEBUG_LOG("displayImage: 准备绘制 %d 像素\n", pixelCount);

    // 清空全屏模式和所有滚动文本
    DEBUG_LOG("displayImage: 清除文本内容...\n");
    clearFullScreenContent();
    freeAllScrollLines();
    freeAllStaticTexts();

    // 进入图片显示模式
    isImageDisplayMode = true;
    DEBUG_LOG("displayImage: 已设置图片显示模式, 耗时: %lu ms\n", millis() - startTime);

    // 清除缓冲区
    DEBUG_LOG("displayImage: 清除缓冲区...\n");
    dma_display->clearScreen();
    DEBUG_LOG("displayImage: 缓冲区清除完成, 耗时: %lu ms\n", millis() - startTime);

    // 将rgb565数据绘制到屏幕
    DEBUG_LOG("displayImage: 开始绘制像素...\n");
    for (int i = 0; i < pixelCount; i++) {
        int x = i % PANEL_RES_X;
        int y = i / PANEL_RES_X;

        // 解析rgb565数据（小端序：低位字节在前）
        uint16_t color = decodedData[i * 2] | ((uint16_t)decodedData[i * 2 + 1] << 8);

        dma_display->drawPixel(x, y, color);

        // 每256个像素喂狗一次，避免看门狗超时
        if (i % 256 == 0) {
            esp_task_wdt_reset();
        }

        // 每1024个像素输出进度（避免过于频繁）
        if (i % 1024 == 0) {
            DEBUG_LOG("displayImage: 绘制进度 %d/%d (%.1f%%), 耗时: %lu ms\n", 
                     i, pixelCount, (i * 100.0) / pixelCount, millis() - startTime);
        }
    }

    DEBUG_LOG("displayImage: 像素绘制完成, 耗时: %lu ms\n", millis() - startTime);

    // 翻转缓冲区显示图像
    DEBUG_LOG("displayImage: 翻转缓冲区...\n");
    dma_display->flipDMABuffer();
    DEBUG_LOG("displayImage: 缓冲区翻转完成, 总耗时: %lu ms\n", millis() - startTime);

    // 释放内存
    DEBUG_LOG("displayImage: 释放内存...\n");
    free(decodedData);

    unsigned long totalTime = millis() - startTime;
    DEBUG_LOG("displayImage: 完成! 总共绘制 %d 像素, 总耗时 %lu ms\n", pixelCount, totalTime);
}