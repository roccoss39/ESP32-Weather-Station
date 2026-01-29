// display_utils.h
#ifndef DISPLAY_UTILS_H
#define DISPLAY_UTILS_H

#include <TFT_eSPI.h>

void clearAndShowMessage(TFT_eSPI& tft, String message, uint16_t color = TFT_WHITE,uint8_t size = 1);

// Simple loading screen (reused for weather refresh and images)
void drawLoadingSpinner(TFT_eSPI& tft, const String& message = "Ladowanie...");


#endif