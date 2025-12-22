#ifndef CURRENT_WEATHER_DISPLAY_H
#define CURRENT_WEATHER_DISPLAY_H

#include <TFT_eSPI.h>
#include "weather/weather_data.h"

// --- FUNKCJE WYŚWIETLANIA POGODY ---
void displayCurrentWeather(TFT_eSPI& tft);

// --- FUNKCJE CACHE ---
bool hasWeatherChanged();
void updateWeatherCache();

// --- FUNKCJE POMOCNICZE (przeniesione z screen_manager.cpp) ---
String shortenDescription(String description);
uint16_t getWindColor(float windKmh);
uint16_t getPressureColor(float pressure);
uint16_t getHumidityColor(float humidity);

// --- NOWY OOP CACHE SYSTEM ---
// Zastąpiono 7 extern variables WeatherCache class
// Forward declaration zamiast include w header
class WeatherCache;

// Singleton instance WeatherCache
WeatherCache& getWeatherCache();

#endif // CURRENT_WEATHER_DISPLAY_H