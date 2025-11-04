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
  unsigned long sunrise = 0;  // Wschód słońca (timestamp Unix)
  unsigned long sunset = 0;   // Zachód słońca (timestamp Unix)
  
  // --- DANE O OPADACH ---
  float rainLastHour = 0;     // mm deszczu w ostatniej godzinie
  float snowLastHour = 0;     // mm śniegu w ostatniej godzinie  
  int cloudiness = 0;         // % zachmurzenia (0-100)
  
  bool isValid = false;
  unsigned long lastUpdate = 0;
};

// Globalna zmienna pogodowa
extern WeatherData weather;
extern unsigned long lastWeatherUpdate;

#endif