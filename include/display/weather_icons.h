#ifndef WEATHER_ICONS_H
#define WEATHER_ICONS_H

#include <TFT_eSPI.h>
#include <Arduino.h>

// --- FUNKCJE IKON POGODOWYCH ---
void drawWeatherIcon(TFT_eSPI& tft, int x, int y, String condition, String iconCode);

#endif