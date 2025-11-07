#ifndef WEATHER_DISPLAY_H
#define WEATHER_DISPLAY_H

#include <TFT_eSPI.h>
#include "weather/weather_data.h"

// --- FUNKCJE WYŚWIETLANIA POGODY ---
void displayWeather(TFT_eSPI& tft);
String shortenDescription(String description);

// --- FUNKCJE CACHE ---
bool hasWeatherChanged();
void updateWeatherCache();

// --- FUNKCJE KOLORÓW ---
uint16_t getWindColor(float windKmh);
uint16_t getPressureColor(float pressure);
uint16_t getHumidityColor(float humidity);

// --- NOWY OOP CACHE SYSTEM ---
// Zastąpiono 7 extern variables WeatherCache class
// Forward declaration zamiast include w header
class WeatherCache;

// Singleton instance WeatherCache
WeatherCache& getWeatherCache();

#endif