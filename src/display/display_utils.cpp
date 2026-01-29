// display_utils.cpp
#include "display_utils.h"

void clearAndShowMessage(TFT_eSPI& tft, String message, uint16_t color, uint8_t size) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(color, TFT_BLACK);
    tft.setTextSize(size);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(message, tft.width() / 2, tft.height() / 2);
}

void drawLoadingSpinner(TFT_eSPI& tft, const String& message) {
    tft.fillScreen(TFT_BLACK);
    int centerX = tft.width() / 2;
    int centerY = tft.height() / 2 - 10;

    // Same simple spinner style as github_image.cpp
    tft.drawCircle(centerX, centerY, 20, TFT_CYAN);
    tft.drawCircle(centerX, centerY, 15, TFT_BLUE);
    tft.drawLine(centerX, centerY, centerX + 15, centerY - 10, TFT_WHITE);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(message, centerX, centerY + 35);
}
