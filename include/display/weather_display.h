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

// Cache dla optymalizacji rysowania
extern float weatherCachePrev_temperature;
extern float weatherCachePrev_feelsLike;
extern float weatherCachePrev_humidity;
extern float weatherCachePrev_windSpeed;
extern float weatherCachePrev_pressure;
extern String weatherCachePrev_description;
extern String weatherCachePrev_icon;

#endif