#ifndef WEATHER_DISPLAY_H
#define WEATHER_DISPLAY_H

#include <TFT_eSPI.h>
#include "weather/weather_data.h"

// --- FUNKCJE CACHE (AKTYWNE) ---
bool hasWeatherChanged();
void updateWeatherCache();

// ❌ FUNKCJE USUNIĘTE - nieużywane (zastąpione przez local_* w screen_manager.cpp):
// void displayWeather(TFT_eSPI& tft);
// String shortenDescription(String description);
// uint16_t getWindColor(float windKmh);
// uint16_t getPressureColor(float pressure);
// uint16_t getHumidityColor(float humidity);

// --- NOWY OOP CACHE SYSTEM ---
// Zastąpiono 7 extern variables WeatherCache class
// Forward declaration zamiast include w header
class WeatherCache;

// Singleton instance WeatherCache
WeatherCache& getWeatherCache();

#endif