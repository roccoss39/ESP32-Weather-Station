#ifndef FORECAST_DISPLAY_H
#define FORECAST_DISPLAY_H

#include <TFT_eSPI.h>
#include "weather/forecast_data.h"

// --- FUNKCJE WYŚWIETLANIA PROGNOZY ---
void displayForecast(TFT_eSPI& tft);
void drawTemperatureGraph(TFT_eSPI& tft, int x, int y, int width, int height);
void drawForecastItems(TFT_eSPI& tft, int y);
void drawForecastSummary(TFT_eSPI& tft, int y);

#endif