#include "DisplayManager.h"
#include "esp_task_wdt.h"

DisplayManager::DisplayManager() :
    _pins({R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN, A_PIN, B_PIN, C_PIN, D_PIN, E_PIN, LAT_PIN, OE_PIN, CLK_PIN}),
    dma_display(nullptr),
    scrollTextTimeDelay(30),
    scrollXMove(-1),
    isAnimationDue(0),
    scrollTextXPosition(PANEL_RES_X),
    scrollTextYPosition(0),
    xOne(0), yOne(0),
    scrollTextWidth(0), scrollTextHeight(0),
    textSize(1),
    isTextWrap(false),
    isScrollText(false),
    scrollTextSpeed(1),
    scrollTextContent(nullptr),
    currentLine(0),
    maxLines(1),
    useMultiLine(false),
    blackColor(0), whiteColor(0), redColor(0), greenColor(0), blueColor(0), yellowColor(0), pinkColor(0)
{
}

DisplayManager::~DisplayManager() {
    if (dma_display) {
        delete dma_display;
        dma_display = nullptr;
    }
    freeScrollText();
}

void DisplayManager::init() {
    HUB75_I2S_CFG mxconfig(
        PANEL_RES_X,   // module width
        PANEL_RES_Y,   // module height
        PANEL_CHAIN,   // Chain length
        _pins          // Pin mapping
    );

    dma_display = new MatrixPanel_I2S_DMA(mxconfig);
    dma_display->begin();
    dma_display->setBrightness(DEFAULT_BRIGHTNESS);

    blackColor = dma_display->color565(0, 0, 0);
    whiteColor = dma_display->color565(255, 255, 255);
    redColor = dma_display->color565(255, 0, 0);
    greenColor = dma_display->color565(0, 255, 0);
    blueColor = dma_display->color565(0, 0, 255);
    yellowColor = dma_display->color565(255, 255, 0);
    pinkColor = dma_display->color565(255, 0, 255);

    dma_display->clearScreen();

    setTextSize(1);
    setTextColor(redColor);
    setTextScrollSpeed(2);
    isAnimationDue = millis();
}

void DisplayManager::update() {
    if (isScrollText && scrollTextContent) {
        unsigned long now = millis();

        if (now > isAnimationDue) {
            // 更新第二缓冲区
            dma_display->flipDMABuffer();

            // 设置滚动时间
            isAnimationDue = now + scrollTextTimeDelay;

            scrollTextXPosition += scrollXMove;

            // 检查文本是否超出屏幕
            dma_display->getTextBounds(scrollTextContent, scrollTextXPosition, scrollTextYPosition, &xOne, &yOne, &scrollTextWidth, &scrollTextHeight);
            if (scrollTextXPosition + scrollTextWidth <= 0) {
                scrollTextXPosition = PANEL_RES_X;
            }

            dma_display->setCursor(scrollTextXPosition, scrollTextYPosition);

            if (useMultiLine && maxLines > 1) clearLine(currentLine);
            else clearAll();

            dma_display->printlnUTF8(scrollTextContent);
        }
    }
}

void DisplayManager::freeScrollText() {
    if (scrollTextContent != NULL) {
        free(scrollTextContent);
        scrollTextContent = NULL;
    }
}

void DisplayManager::clearAll() {
    dma_display->clearScreen();
}

void DisplayManager::clearArea(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    dma_display->fillRect(x, y, width, height, blackColor);
}

void DisplayManager::clearLine(uint16_t line) {
    dma_display->fillRect(0, (line-1) * textSize * 16, PANEL_RES_X, textSize * 16, blackColor);
}

void DisplayManager::setTextSize(int size) {
    textSize = size;
    dma_display->setTextSize(textSize);
    // 计算最大行数 (字体高度是16 * textSize)
    maxLines = PANEL_RES_Y / (textSize * 16);
    if (maxLines < 1) maxLines = 1;
}

void DisplayManager::setMultiLineMode(bool enable) {
    useMultiLine = enable;
}

void DisplayManager::setLine(int line) {
    if (line >= 0 && line < maxLines) {
        currentLine = line;
    } else {
        currentLine = 0;
    }
}

void DisplayManager::setTextColor(uint16_t color) {
    dma_display->setTextColor(color);
}

void DisplayManager::setTextScrollSpeed(int speed) {
    scrollTextSpeed = speed;
    switch (scrollTextSpeed) {
        case 1:
            scrollXMove = SCROLL_OFFSET_LOW;
            scrollTextTimeDelay = SCROLL_TIME_DELAY_LOW;
            break;
        case 2:
            scrollXMove = SCROLL_OFFSET_MEDIUM;
            scrollTextTimeDelay = SCROLL_TIME_DELAY_MEDIUM;
            break;
        case 3:
            scrollXMove = SCROLL_OFFSET_FAST;
            scrollTextTimeDelay = SCROLL_TIME_DELAY_FAST;
            break;
        default:
            break;
    }
}

void DisplayManager::displayText(const char *textContent, bool isScroll) {
    isScrollText = false;
    // 防止不同步，导致内促出错
    delay(50);
    freeScrollText();
    setMultiLineMode(false);

    clearAll();
    // dma_display->setTextColor(color);
    // 如果滚动
    if (isScroll) {
        isTextWrap = false;
        dma_display->setTextWrap(false);
        int len = strlen(textContent) + 1;  // +1 for '\0'
        scrollTextContent = (char *)malloc(len * sizeof(char));
        if (scrollTextContent != NULL) {
            strcpy(scrollTextContent, textContent);
        }
        // 设置滚动时的初始Y坐标
        scrollTextYPosition = 0;
    } else {
        dma_display->setCursor(0, 0);
        isTextWrap = true;
        dma_display->setTextWrap(true);
        dma_display->printlnUTF8(textContent);
    }
    isScrollText = isScroll;
}

void DisplayManager::displayText(const char *textContent, bool isScroll, int line) {
    if (!isScroll) {
        displayText(textContent, isScroll);
        return;
    }

    if (line < 1 || line > maxLines) line = 1;

    setMultiLineMode(true);

    currentLine = line;
    isScrollText = false;
    // 防止不同步，导致内促出错
    delay(50);
    freeScrollText();

    // 滚动文本：清除指定行区域
    int yPosition = (line-1) * textSize * 16;
    dma_display->fillRect(0, yPosition, PANEL_RES_X, textSize * 16, blackColor);

    isTextWrap = false;
    dma_display->setTextWrap(false);
    int len = strlen(textContent) + 1;  // +1 for '\0'
    scrollTextContent = (char *)malloc(len * sizeof(char));
    if (scrollTextContent != NULL) {
        strcpy(scrollTextContent, textContent);
    }
    // 设置滚动时的初始Y坐标
    scrollTextYPosition = yPosition;
    isScrollText = isScroll;
}