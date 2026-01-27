// display_utils.cpp
#include "display_utils.h"

void clearAndShowMessage(TFT_eSPI& tft, String message, uint16_t color, uint8_t size) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(color, TFT_BLACK);
    tft.setTextSize(size);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(message, tft.width() / 2, tft.height() / 2);
}