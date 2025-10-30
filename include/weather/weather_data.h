#ifndef WEATHER_DATA_H
#define WEATHER_DATA_H

#include <Arduino.h>

// --- STRUKTURA DANYCH POGODOWYCH ---
struct WeatherData {
  float temperature = 0;
  float feelsLike = 0;        // Temperatura odczuwalna
  String description = "";
  float humidity = 0;
  float windSpeed = 0;
  float pressure = 0;         // Ciśnienie w hPa
  String icon = "";
  bool isValid = false;
  unsigned long lastUpdate = 0;
};

// Globalna zmienna pogodowa
extern WeatherData weather;
extern unsigned long lastWeatherUpdate;

#endif